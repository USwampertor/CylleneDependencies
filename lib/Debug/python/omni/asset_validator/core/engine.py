from __future__ import annotations
import asyncio
import collections
import fnmatch
from asyncio import Task
from concurrent.futures import ThreadPoolExecutor, Future
from typing import Callable, Optional, OrderedDict, Tuple, Type, List, Coroutine, TypeVar
import traceback

import omni.client

from pxr import Usd

import carb
import carb.settings

from .complianceChecker import ComplianceChecker, BaseRuleChecker
from .autofix import Issue, IssueSeverity, Results

T = TypeVar("T")

AssetType = TypeVar(
    "AssetType",
    str,
    Usd.Stage,
)
"""
 A typedef for the assets that ValidationEngine can process.
 When it is a `str`, it is expected to be an Omniverse URI that can be accessed via the Omni Client Library.
 When it is a `Usd.Stage`, it will be validated directly without ever locating a file on disk/server.
 Note using a live Stage necessarily bypasses some validations (i.e. file I/O checks)
"""

AssetLocatedCallback = TypeVar(
    "AssetLocatedCallback",
    bound=Callable[[AssetType], None],
)
"""
A typedef for the notification of asset(s) founds during :py:meth:`ValidationEngine.validate_with_callbacks`.
It is invoked at the beginning of asset validation.

Args:
    asset (AssetType): The asset type located.
"""

AssetValidatedCallback = TypeVar(
    "AssetValidatedCallback",
    bound=Callable[[Results], None],
)
"""
A typedef for the notification of asset results found during :py:meth:`ValidationEngine.validate_with_callbacks`.

Args:
    results (Results): The results for a specific asset.
"""


class ValidationEngine:
    """An engine for running rule-checkers on a given Omniverse Asset.

    Rules are :py:class:`BaseRuleChecker` derived classes which perform specific validation checks over various aspects
    of a USD layer/stage. Rules must be registered with the :py:class:`ValidationRulesRegistry` and subsequently enabled
    on each instance of the :py:class:`ValidationEngine`.

    Validation can be performed asynchronously (using either :py:meth:`validate_async` or :py:meth:`validate_with_callbacks`)
    or blocking (via :py:meth:`validate`).

    Example:
        Construct an engine and validate several assets using the default-enabled rules:

        .. code-block:: python

            import omni.asset_validator.core

            engine = omni.asset_validator.core.ValidationEngine()

            # Validate a single Omniverse file
            print( engine.validate('omniverse://localhost/NVIDIA/Samples/Astronaut/Astronaut.usd') )

            # Validate a local file on disk. Windows paths must be pre-converted to Omniverse compatible paths
            import omni.client
            ( status, localFile ) = omni.client.get_local_file('C:\\\\Users\\\\Public\\\\Documents\\\\test.usda')
            if status == omni.client.Result.OK :
                print( engine.validate(localFile) )

            # Search an Omniverse folder and recursively validate all USD files asynchronously
            # note a running asyncio EvenLoop is required
            task = engine.validate_with_callbacks(
                'omniverse://localhost/NVIDIA/Assets/ArchVis/Industrial/Containers/',
                asset_located_fn = lambda url: print(f'Validating "{url}"'),
                asset_validated_fn = lambda result: print(result),
            )
            task.add_done_callback(lambda task: print('validate_with_callbacks complete'))

            # Perform the same search & validate but await the results
            import asyncio
            async def test(url):
                results = await engine.validate_async(url)
                for result in results:
                    print(result)
            asyncio.ensure_future(test('omniverse://localhost/NVIDIA/Assets/ArchVis/Industrial/Containers/'))

            # Load a a layer onto a stage and validate it in-memory, including any unsaved edits
            from pxr import Usd, Kind
            stage = Usd.Stage.Open('omniverse://localhost/NVIDIA/Samples/Astronaut/Astronaut.usd')
            prim = stage.DefinePrim(f'{stage.GetDefaultPrim().GetPath()}/MyCube', 'cube')
            Usd.ModelAPI(prim).SetKind(Kind.Tokens.component)
            print( engine.validate(stage) )

            # Validate the current stage in any Kit based app (eg Create, View)
            import omni.usd
            print( engine.validate( omni.usd.get_context().get_stage() ) )

    Args:
        initRules: Flag to default-enable Rules on this engine based on carb settings. Clients may wish to opt-out
            of this behavior to gain finer control over rule enabling at runtime.
    """

    AssetType: Type = AssetType
    """
        .. deprecated:: 0.4.0 Use :py:class:`AssetType`.
    """
    AssetLocatedCallback: Type = AssetLocatedCallback
    """
        .. deprecated:: 0.4.0 Use :py:class:`AssetLocatedCallback`.
    """
    AssetValidatedCallback: Type = AssetValidatedCallback
    """
        .. deprecated:: 0.4.0 Use :py:class:`AssetValidatedCallback`.
    """

    def __init__(self, initRules: bool=True) -> None:
        self.__initRules = initRules
        self.__enabledRules = []
        self.__tasks = set()

    @staticmethod
    def isAssetSupported(asset: AssetType) -> bool:
        """
        Determines if the provided asset can be validated by the engine.

        Args:
            asset (AssetType): A single Omniverse Asset pointing to a file URI, folder/container URI, or a live `Usd.Stage`.

        Returns:
            Whether the provided asset can be validated by the engine.
        """
        if isinstance(asset, Usd.Stage):
            return True

        if not asset:
            return False

        uri = omni.client.break_url(asset)
        if not uri or not uri.path:
            return False

        return Usd.Stage.IsSupportedFile(uri.path)

    @staticmethod
    def describe(asset: AssetType) -> str:
        """Provides a description of an Omniverse Asset.

        Args:
            asset (AssetType): A single Omniverse Asset pointing to a file URI, folder/container URI, or a live `Usd.Stage`.

        Returns:
            The `str` description of the asset that was validated.
        """
        return Usd.Describe(asset) if isinstance(asset, Usd.Stage) else asset

    def enableRule(self, rule: Type[BaseRuleChecker]) -> None:
        """
        Enable a given rule on this engine.

        This gives control to client code to enable rules one by one. Rules must be :py:class:`BaseRuleChecker` derived
        classes, and should be registered with the :py:class:`ValidationRulesRegistry` before they are enabled on
        this engine.

        Note many rules may have been pre-enabled if the engine was default-constructed with `initRules=True`.

        Args:
            rule (Type[BaseRuleChecker]): A `BaseRuleChecker` derived class to be enabled
        """
        self.__enabledRules.append(rule)

    def validate(self, asset: AssetType) -> Results:
        """
        Run the enabled rules on the given asset. **(Blocking version)**

        .. note::
            Validation of folders/container URIs is not supported in the blocking version. Use
            :py:meth:`validate_async` or :py:meth:`validate_with_callbacks` to recursively validate a folder.

        Args:
            asset (AssetType): A single Omniverse Asset pointing to a file URI or a live `Usd.Stage`.

        Returns:
            All issues reported by the enabled rules.
        """
        desc: str = self.describe(asset)
        carb.log_info(f'Validating "{desc}"')

        if isinstance(asset, Usd.Stage):
            return self.__validate(asset)

        status, entry = omni.client.stat(asset)
        if status != omni.client.Result.OK:
            return self.__accessFailure(asset, status)

        if entry.flags & omni.client.ItemFlags.CAN_HAVE_CHILDREN:
            raise RuntimeError(
                "ValidationEngine: Synchronous validation of folders/containers is not available. "
                "Use `validate_async` or `validate_with_callbacks`"
            )

        if not self.isAssetSupported(asset):
            return Results(
                asset=desc,
                issues=[Issue(
                    severity=IssueSeverity.ERROR,
                    message=f'Validation requires a readable USD file, not "{desc}".'
                )]
            )

        return self.__validate(asset)

    async def validate_async(self, asset: AssetType) -> List[Results]:
        """
        Asynchronously run the enabled rules on the given asset. **(Concurrent Version)**

        If the asset is a folder/container URI it will be recursively searched for individual asset files and each
        applicable URI will be validated, with all results accumulated and indexed alongside the respective asset.

        .. note::
            Even a single asset will return a list of :py:class:`Results`, so it must be indexed via
            `results[0].asset`, `results[0].failures`, etc

        Args:
            asset (AssetType): A single Omniverse Asset. Note this can be a file URI, folder/container URI,
            or a live `Usd.Stage`.

        Returns:
            All issues reported by the enabled rules, index aligned with their respective asset.
        """
        carb.log_info(f'Validating "{self.describe(asset)}" asynchronously')

        if isinstance(asset, Usd.Stage):
            return [await self.__validate_async(asset=asset)]

        status, entry = await omni.client.stat_async(asset)
        if status != omni.client.Result.OK:
            return [self.__accessFailure(asset, status)]

        all_assets = await self.__checkEntry(asset, entry.flags)
        return await self.__validate_all_async(all_assets=all_assets, asset_validated_fn=None)

    def validate_with_callbacks(
        self,
        asset: AssetType,
        asset_located_fn: Optional[AssetLocatedCallback] = None,
        asset_validated_fn: Optional[AssetValidatedCallback] = None
    ) -> Task:
        """
        Asynchronously run the enabled rules on the given asset. **(Callbacks Version)**

        If the asset is validate-able (e.g. a USD layer file), `asset_located_fn` will be invoked before validation
        begins. When validation completes, `asset_validated_fn` will be invoked with the results.

        If the asset is a folder/container URI it will be recursively searched for individual asset files and each
        applicable URL will be validated, with `asset_located_fn` and `asset_validated_fn` being invoked once per
        validate-able asset.

        Args:
            asset: A single Omniverse Asset. Note this can be a file URI, folder/container URI, or a live `Usd.Stage`.
            asset_located_fn: A callable to be invoked upon locating an individual asset. If `asset` is a single
                validate-able asset (e.g. a USD layer file) `asset_located_fn` will be called once. If `asset` is a
                folder/container URI `asset_located_fn` will be called once per validate-able asset within the container
                (e.g. once per USD layer file). Signature must be `cb(AssetType)` where str is the url of the located asset.
            asset_validated_fn: A callable to be invoked when validation of an individual asset has completed. If `asset`
                is itself a single validate-able asset (e.g. a USD layer file) `asset_validated_fn` will be called once.
                If `asset` is a folder/container `asset_validated_fn` will be called once per validate-able asset within
                the container (e.g. once per USD layer file). Signature must be `cb(results)`.

        Returns:
            A task to control execution.
        """
        carb.log_info(f'Validating "{self.describe(asset)}" asynchronously')
        return self.__run_in_background(
            coroutine=self.__validate_with_callbacks_async(
                asset=asset, asset_located_fn=asset_located_fn, asset_validated_fn=asset_validated_fn
            )
        )

    def __validate(self, asset: AssetType) -> Results:
        checker: ComplianceChecker = self.__complianceChecker()
        desc: str = self.describe(asset)
        try:
            if isinstance(asset, Usd.Stage):
                checker.CheckStageCompliance(usdStage=asset)
            else:
                checker.CheckCompliance(inputFile=asset)
        except: # noqa
            return Results(
                asset=desc,
                issues=[Issue(
                    severity=IssueSeverity.ERROR,
                    message=f'Failed to Open "{desc}". See traceback for details.\n{traceback.format_exc()}'
                )]
            )
        else:
            return Results(
                asset=desc,
                issues=checker.GetErrors() + checker.GetWarnings() + checker.GetFailedChecks(),
            )

    def __run_in_background(self, coroutine: Coroutine) -> asyncio.Task:
        # Enqueue a coroutine. Save reference, to avoid a task disappearing mid-execution.
        task: asyncio.Task = asyncio.ensure_future(coroutine)
        self.__tasks.add(task)
        task.add_done_callback(self.__tasks.discard)
        return task

    async def __call_async(self, callback: Optional[Callable[[T], None]], arg: T) -> None:
        # run_in_executor to avoid blocking.
        if not callback:
            return
        loop = asyncio.get_running_loop()
        task = loop.run_in_executor(None, callback, arg)
        await task

    async def __validate_async(self, asset: AssetType) -> Results:
        # run in executor to avoid blocking.
        with ThreadPoolExecutor(max_workers=1) as executor:
            future: Future = executor.submit(self.__validate, asset)
            result: Results = future.result()
            return result

    async def __validate_all_async(
        self,
        all_assets: List[AssetType],
        asset_validated_fn: Optional[AssetValidatedCallback]
    ) -> List[Results]:
        # run in executor to avoid blocking.
        with ThreadPoolExecutor(max_workers=1) as executor:
            results: List[Results] = []
            for asset in all_assets:
                future: Future = executor.submit(self.__validate, asset)
                result: Results = future.result()
                await self.__call_async(asset_validated_fn, result)
                results.append(result)
            return results

    async def __validate_with_callbacks_async(
        self,
        asset: AssetType,
        asset_located_fn: Optional[AssetLocatedCallback],
        asset_validated_fn: Optional[AssetValidatedCallback]
    ) -> None:
        if isinstance(asset, Usd.Stage):
            await self.__call_async(asset_located_fn, asset)
            result = self.__validate(asset)  # Beware this is correct, otherwise it breaks parallel threads.
            await self.__call_async(asset_validated_fn, result)
        else:
            # initial check to provide feedback if the url is invalid. note this will trigger callbacks only
            # if the asset is invalid, otherwise the primary locate & validate tasks will handle it as normal
            await self.__accessFailure_with_callbacks(asset, asset_located_fn, asset_validated_fn)
            all_assets = await self.__checkEntry(asset, asset_located_fn=asset_located_fn)
            await self.__validate_all_async(all_assets, asset_validated_fn)

    async def __accessFailure_with_callbacks(
        self,
        url: str,
        asset_located_fn: Optional[AssetLocatedCallback],
        asset_validated_fn: Optional[AssetValidatedCallback],
    ) -> None:
        status, entry = await omni.client.stat_async(url)
        if status != omni.client.Result.OK:
            # call located even though we didn't really... this is necessary to inform
            # client code that this URL was analyzed (i.e. to the UI can add a section)
            await self.__call_async(asset_located_fn, url)
            await self.__call_async(asset_validated_fn, self.__accessFailure(url, status))

    async def __checkEntry(
        self,
        url: str,
        flags: omni.client.ItemFlags = None,
        asset_located_fn: Optional[AssetLocatedCallback] = None
    ) -> List[str]:
        if flags is None:
            status, entry = await omni.client.stat_async(url)
            if status != omni.client.Result.OK:
                return []
            flags = entry.flags

        if flags & omni.client.ItemFlags.READABLE_FILE and self.isAssetSupported(url):
            await self.__call_async(asset_located_fn, url)
            return [url]
        elif flags & omni.client.ItemFlags.CAN_HAVE_CHILDREN:
            return await self.__checkChildren(url, asset_located_fn)
        else:
            return []

    async def __checkChildren(
        self,
        url: str,
        asset_located_fn: Optional[AssetLocatedCallback]
    ) -> List[str]:
        carb.log_info(f'Searching for USD files in "{url}"...')
        status, entries = await omni.client.list_async(url)
        if status != omni.client.Result.OK:
            return []

        b_url = omni.client.break_url(url)
        all_assets: List[str] = []
        for entry in entries:
            entry_url = omni.client.make_url(
                b_url.scheme, b_url.user, b_url.host, b_url.port,  # unchanged
                f'{b_url.path}/{entry.relative_path}',
                b_url.query, b_url.fragment  # unchanged
            )
            assets: List[str] = await self.__checkEntry(entry_url, entry.flags, asset_located_fn)
            all_assets.extend(assets)

        return all_assets

    def __accessFailure(self, url: str, status: omni.client.Result) -> Results:
        return Results(
            asset=url,
            issues=[Issue(severity=IssueSeverity.ERROR, message=f'Accessing "{url}" failed with {status}')]
        )

    def __complianceChecker(self) -> ComplianceChecker:
        checker = ComplianceChecker()
        if self.__initRules:
            for category in ValidationRulesRegistry.categories(enabledOnly=True):
                for Rule in ValidationRulesRegistry.rules(category, enabledOnly=True):
                    checker.AddRule(Rule)

        for Rule in self.__enabledRules:
            checker.AddRule(Rule)

        return checker


class ValidationRulesRegistry:
    """A registry enabling external clients to add new rules to the engine.

    Rules **must derive from** :py:class:`BaseRuleChecker` and should re-implement the necessary
    virtual methods required for their specific check, as well as documenting with an
    appropriate user-facing message.

    Rules are registered to specific `categories` (`str` labels) to provide bulk enabling/disabling via carb settings
    and to make logical grouping in UIs or other documentation easier.

    Example:
        Define a new Rule that requires all prims to be meshes or xforms (e.g. if your app only handles these types)
        and register it with the validation framework under a custom category:

        .. code-block:: python

            import omni.asset_validator.core

            @omni.asset_validator.core.registerRule("MyOwnRules")
            class MyRuleChecker(omni.asset_validator.core.BaseRuleChecker):
                '''Check that all prims are meshes for xforms'''

                def CheckPrim(self, prim) -> None:
                    self._Msg("Checking prim <%s>." % prim.GetPath())
                    if prim.GetTypeName() not in ("Mesh", "Xform"):
                        self._AddFailedCheck(
                            f"Prim <{prim.GetPath()}> has unsupported type '{prim.GetTypeName()}'."
                        )

    By default all rules that ship with USD itself are registered into "Basic" and "ARKit" categories, though "ARKit"
    rules have been default disabled via carb settings.
    """

    @staticmethod
    def categories(enabledOnly: bool=False) -> Tuple[str, ...]:
        """Query all registered categories

        Args:
            enabledOnly: Filter the results to only categories that are enabled (via carb settings)

        Returns:
            A tuple of category strings that can be used in :py:meth:`rules`
        """
        if enabledOnly:
            return ValidationRulesRegistry.__enabledCategories()
        return tuple(ValidationRulesRegistry.__rules.keys())

    @staticmethod
    def rules(category: str, enabledOnly: bool=False) -> Tuple[Type[BaseRuleChecker], ...]:
        """Query all registered rules in a given category

        Args:
            category: Filter for rules only in a specific category
            enabledOnly: Filter the results to only rules that are enabled (via carb settings)

        Returns:
            A tuple of BaseRuleChecker derived classes
        """
        if enabledOnly:
            return ValidationRulesRegistry.__enabledRules(category)
        return tuple(ValidationRulesRegistry.__rules[category])

    @staticmethod
    def registerRule(rule: Type[BaseRuleChecker], category: str) -> None:
        """Register a new Rule to a specific category

        Args:
            rule: A BaseRuleChecker derived class that implements a specific check
            category: The label with which this rule will be associated
        """
        for key, rules in ValidationRulesRegistry.__rules.items():
            if rule in rules:
                ValidationRulesRegistry.__rules[key].remove(rule)
        if category not in ValidationRulesRegistry.__rules:
            ValidationRulesRegistry.__rules[category] = []
        ValidationRulesRegistry.__rules[category].append(rule)

    @staticmethod
    def deregisterRule(rule: Type[BaseRuleChecker]) -> None:
        """Remove a specific Rule from the registry

        For convenience it is not required to specify the category, the rule will be removed from all categories,
        and subsequent empty categories will be removed from the registry.

        Args:
            rule: A BaseRuleChecker derived class that implements a specific check
        """
        for key, rules in ValidationRulesRegistry.__rules.items():
            if rule in rules:
                ValidationRulesRegistry.__rules[key].remove(rule)
                if not ValidationRulesRegistry.__rules[key]:
                    del ValidationRulesRegistry.__rules[key]

    @staticmethod
    def rule(name: str) -> Optional[Type[BaseRuleChecker]]:
        """Query a registered rule by class name

        Args:
            name: The exact (case sensitive) class name of a previously registered rule

        Returns:
            A BaseRuleChecker derived class or None
        """
        for _, rules in ValidationRulesRegistry.__rules.items():
            for rule in rules:
                if name == rule.__name__:
                    return rule

        return None

    @staticmethod
    def category(rule: Type[BaseRuleChecker]) -> str:
        """Query the category of a specific rule

        Args:
            rule: A previously registered BaseRuleChecker derived class

        Returns:
            A valid category name or empty string
        """
        for key, rules in ValidationRulesRegistry.__rules.items():
            if rule in rules:
                return key

        return ""

    @staticmethod
    def __enabledCategories() -> Tuple[str, ...]:
        enabledCategories = carb.settings.get_settings().get("exts/omni.asset_validator.core/enabledCategories") or ["*"]
        disabledCategories = carb.settings.get_settings().get("exts/omni.asset_validator.core/disabledCategories") or ["ARKit"]

        categories = []
        for category in ValidationRulesRegistry.__rules.keys():
            # skip disabled categories unless they are explicitly enabled (e.g. by a downstream ext/app setting)
            if category in enabledCategories :
                categories.append(category)
                continue

            if not any(fnmatch.fnmatchcase(category, pattern) for pattern in disabledCategories):
                categories.append(category)

        return tuple(categories)

    @staticmethod
    def __enabledRules(category: str) -> Tuple[Type[BaseRuleChecker], ...]:
        if category not in ValidationRulesRegistry.__enabledCategories():
            return tuple()

        enabledRules = carb.settings.get_settings().get("exts/omni.asset_validator.core/enabledRules") or ["*"]
        disabledRules = carb.settings.get_settings().get("exts/omni.asset_validator.core/disabledRules") or [""]

        rules = []
        for Rule in ValidationRulesRegistry.__rules[category]:
            # skip disabled rules unless they are explicitly enabled (e.g. by a downstream ext/app setting)
            if Rule.__name__ in enabledRules:
                rules.append(Rule)
                continue

            if not any(fnmatch.fnmatchcase(Rule.__name__, pattern) for pattern in disabledRules):
                rules.append(Rule)

        return tuple(rules)

    __rules: OrderedDict = collections.OrderedDict()


############################################################
# Register rules from the native ComplianceChecker
############################################################


for rule in ComplianceChecker.GetBaseRules():
    ValidationRulesRegistry.registerRule(rule, 'Basic')

for rule in ComplianceChecker.GetARKitRules():
    ValidationRulesRegistry.registerRule(rule, 'ARKit')


def registerRule(category: str) -> Callable[[Type[BaseRuleChecker]]: Type[BaseRuleChecker]]:
    """Decorator. Register a new :py:class:`BaseRuleChecker` to a specific category.

    Example:
        Register MyRule into the category "MyCategory" so that becomes part of the
        default initialized ValidationEngine.

    .. code-block:: python

        @registerRule("MyCategory")
        class MyRule(BaseRuleChecker):
            pass

    Args:
        category (str): The label with which this rule will be associated
    """

    def _registerRule(ruleClass: Type[BaseRuleChecker]) -> Type[BaseRuleChecker]:
        """
        Take the rule class and register under specific category.
        Return the rule class un altered.
        """
        ValidationRulesRegistry.registerRule(ruleClass, category)
        return ruleClass

    return _registerRule
