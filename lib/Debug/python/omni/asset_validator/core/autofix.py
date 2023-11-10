"""
Auto fix framework.

The idea behind an Auto Fix should be simple. Users should be able to `validate`, get the `Results` object,
get the `autofixes` and apply them (prior filtering).

A simple use case is as follows:

.. code-block:: python

    import omni.asset_validator.core

    engine = omni.asset_validator.core.ValidationEngine()
    results = engine.validate('omniverse://localhost/NVIDIA/Samples/Astronaut/Astronaut.usd')
    issues = results.issues()

    fixer = omni.asset_validator.core.IssueFixer('omniverse://localhost/NVIDIA/Samples/Astronaut/Astronaut.usd')
    fixer.fix(issues)
    fixer.save()

Auto Fixes may or may not be applied at all, this depends completely on the user. As such Auto Fixes may be applied
post factum (i.e. after Validation), as such we need to take the following into account:

- All USD objects accessed during validation may lose references, we need to keep the references somehow.
- We need to keep track of the actions we would have performed on the those references.

To keep references to all prims we visited we create multiple `Identifiers`. Identifiers contain the key of the
object to be accessed. Callables will tell us what action perform on those references. The idea is to create an AST
of post mortum actions.

See Also `BaseRuleChecker`
"""
from __future__ import annotations
from enum import Enum
from functools import lru_cache
from typing import (
    List,
    Optional,
    Callable,
    Union,
    Generic,
    TypeVar,
    Any,
    Set,
)

import attr
from attr import define, field, asdict, frozen
from attr.validators import optional, instance_of, is_callable, deep_iterable

import omni.client
from pxr import Usd, Sdf, Tf, Pcp

# Type section

AtType = TypeVar(
    "AtType",
    Sdf.Layer,
    Usd.Stage,
    Usd.Prim,
    Usd.Property,
    Usd.Attribute,
    Usd.Relationship,
    Sdf.PrimSpec,
    Sdf.PropertySpec,
)
"""
Location of an issue or a fix.

Can be any of:
  - :py:class:`pxr.Sdf.Layer`
  - :py:class:`pxr.Usd.Stage`
  - :py:class:`pxr.Usd.Prim`
  - :py:class:`pxr.Usd.Property`
  - :py:class:`pxr.Usd.Attribute`
  - :py:class:`pxr.Usd.Relationship`
  - :py:class:`pxr.Sdf.PrimSpec`
  - :py:class:`pxr.Sdf.PropertySpec`
"""

RuleType = TypeVar("RuleType")
"""Any Checker is a subclass of BaseRuleChecker."""


@define
class _KeyValue:
    key = field()
    value = field()


def _dumps(obj: Any, indent: int = 0) -> str:
    """
    Internal. Converts a class into pretty string representation.
    """

    def tab(text) -> str:
        space = ' ' * (4 * indent)
        return f"{space}{text}"

    parts = []
    if isinstance(obj, _KeyValue):
        parts.append(tab(f"{obj.key.lstrip('_')}={_dumps(obj.value, indent)}"))
    elif attr.has(obj):
        parts.append(tab(f"{obj.__class__.__name__}("))
        parts.append(",\n".join(_dumps(_KeyValue(k, v), indent + 1)
                                for k, v in asdict(obj, recurse=False).items()))
        parts.append(tab(")"))
    elif type(obj) == list:
        parts.append("[")
        parts.append(",\n".join(_dumps(item, indent + 1) for item in obj))
        parts.append(tab("]"))
    elif type(obj) == str:
        parts.append(f'"{obj}"')
    elif type(obj) == type:
        parts.append(obj.__name__)
    elif callable(obj):
        parts.append(obj.__name__)
    else:
        parts.append(str(obj))
    return "\n".join(parts)


# Identifier section

class Identifier(Generic[AtType]):
    """
    An Identifier is a stateful representation of an Usd object (i.e. Usd.Prim). A identifier can convert back to a
    live Usd object.
    """

    @classmethod
    def from_(cls, obj: AtType) -> Identifier[AtType]:
        """
        Args:
            obj (AtType): An Usd Object.

        Returns:
            A stateful representation of an Usd object.
        """
        raise NotImplementedError()

    def restore(self, stage: Usd.Stage) -> AtType:
        """
        Convert this stateful identifier to a live object.

        Args:
            stage (Usd.Stage): The stage to use to restore the object.

        Returns:
            An Usd object.
        """
        raise NotImplementedError()

    def as_str(self) -> str:
        """
        Returns:
            Pretty print representation of this object. Useful for testing.
        """
        raise NotImplementedError()

    def get_layer_ids(self) -> List[LayerId]:
        """
        Returns:
            The list of all possible layer ids associated to this identifier.
        """
        return []

    def get_spec_ids(self) -> List[SpecId]:
        """
        Returns:
            The list of all possible prim specs i.e., path and layer ids associated to this identifier.
        """
        return []


@frozen
class LayerId(Identifier[Sdf.Layer]):
    """
    A unique identifier to layer, i.e. identifier.

    Attributes:
        identifier (str): The unique identifier of this layer.
    """
    identifier: str = field(validator=instance_of(str))

    @classmethod
    def from_(cls, layer: Sdf.Layer) -> LayerId:
        return LayerId(
            identifier=layer.identifier
        )

    def restore(self, stage: Usd.Stage) -> LayerId:
        return Sdf.Layer.FindOrOpen(self.identifier)

    def get_layer_ids(self) -> List[LayerId]:
        return [self]

    def get_spec_ids(self) -> List[SpecId]:
        return [SpecId(layer_id=self, path=Sdf.Path.absoluteRootPath)]


@frozen
class StageId(Identifier[Usd.Stage]):
    """
    A unique identifier to stage, i.e. identifier.

    Attributes:
        root_layer (LayerId): Identifier representing the root layer.
    """
    root_layer: LayerId = field(validator=instance_of(LayerId))

    @classmethod
    def from_(cls, stage: Usd.Stage) -> StageId:
        return StageId(
            root_layer=LayerId.from_(stage.GetRootLayer()),
        )

    @property
    def identifier(self) -> str:
        """
        .. deprecated:: 0.4.0 Use :py:attr:`StageId.root_layer` attribute.
        """
        return self.root_layer.identifier

    @property
    def stage_id(self) -> StageId:
        return self

    def restore(self, stage: Usd.Stage) -> Usd.Stage:
        if stage.GetRootLayer().identifier != self.root_layer.identifier:
            raise ValueError("The supplied stage do not corresponds to current stage.")
        return stage

    def as_str(self) -> str:
        return f"Stage <{self.root_layer.identifier}>"

    def get_layer_ids(self) -> List[LayerId]:
        return [self.root_layer]

    def get_spec_ids(self) -> List[SpecId]:
        return self.root_layer.get_spec_ids()


@frozen
class SpecId(Identifier[Sdf.Spec]):
    """
    A snapshot of a Prim Specification.

    Attributes:
        layer_id (LayerId): The layer where this identifier exists.
        path (Sdf.Path): The path to this specification.
    """
    layer_id: LayerId = field(validator=instance_of(LayerId))
    path: Sdf.Path = field()

    @classmethod
    def from_(cls, spec: Sdf.Spec) -> SpecId:
        return SpecId(
            layer_id=LayerId.from_(spec.layer),
            path=spec.path,
        )

    def restore(self, stage: Usd.Stage) -> Sdf.Spec:
        layer: Sdf.Layer = self.layer_id.restore(stage)
        spec: Sdf.PrimSpec = layer.GetPrimAtPath(self.path.GetPrimPath())
        return spec

    def get_layer_ids(self) -> List[LayerId]:
        return [self.layer_id]

    def get_spec_ids(self) -> List[SpecId]:
        return [self]


NodeId = SpecId
"""
.. deprecated:: 0.4.0 Please use ``SpecId`` instead.
"""


@frozen
class SpecIdList:
    """
    A snapshot of a list of PcpNodeRefs.

    Attributes:
        root_path (Sdf.Path): The prim path in stage.
        spec_ids (List[SpecId]): The list of prim specifications.
    """
    root_path: Sdf.Path = field()
    spec_ids: List[SpecId] = field(validator=deep_iterable(instance_of(SpecId)), hash=False)

    @classmethod
    def from_(cls, prim: Usd.Prim) -> SpecIdList:
        nodes: List[SpecId] = []
        for spec in prim.GetPrimStack():
            if spec.layer.anonymous:
                continue
            nodes.append(SpecId.from_(spec))
        return SpecIdList(root_path=prim.GetPrimPath(), spec_ids=nodes)

    def __iter__(self):
        return iter(self.spec_ids)

    @property
    def nodes(self) -> List[NodeId]:
        """
        .. deprecated:: 0.4.0 Please use :py:attr:`SpecIdList.spec_ids`.
        """
        return self.spec_ids


NodeIdList = SpecIdList
"""
.. deprecated:: 0.4.0 Please use ``SpecIdList`` instead.
"""


@frozen
class PrimId(Identifier[Usd.Prim]):
    """
    A unique identifier of a prim, i.e. a combination of Stage definition and a list of Specs.

    Attributes:
        stage_id (StageId): An identifier to the stage this prim exists.
        spec_ids (SpecIdList): The list of specifications as found in the stage.
    """
    stage_id: StageId = field(validator=instance_of(StageId))
    spec_ids: SpecIdList = field(validator=instance_of(SpecIdList))

    @classmethod
    def from_(cls, prim: Usd.Prim) -> PrimId:
        return PrimId(
            stage_id=StageId.from_(prim.GetStage()),
            spec_ids=SpecIdList.from_(prim),
        )

    def restore(self, stage: Usd.Stage) -> Usd.Prim:
        stage = self.stage_id.restore(stage)
        return stage.GetPrimAtPath(self.path)

    @property
    def path(self) -> str:
        return self.spec_ids.root_path

    def as_str(self) -> str:
        return f"Prim <{self.path}>"

    @property
    def nodes(self) -> NodeIdList:
        """
        .. deprecated:: 0.4.0 Please use :py:attr:`PrimId.spec_ids`.
        """
        return self.spec_ids

    def get_layer_ids(self) -> List[LayerId]:
        result: List[LayerId] = []
        for node_id in self.spec_ids:
            result.extend(node_id.get_layer_ids())
        return result

    def get_spec_ids(self) -> List[SpecId]:
        result: List[SpecId] = []
        for node_id in self.spec_ids:
            result.extend(node_id.get_spec_ids())
        return result


@frozen
class PropertyId(Identifier[Usd.Attribute]):
    """
    A unique identifier of a property, i.e. a combination of prim definition and property path.

    Attributes:
        prim_id: (PrimId) An identifier to the prim containing this property.
        path (Sdf.Path): The path to this property.
    """
    prim_id: PrimId = field(validator=instance_of(PrimId))
    path: Sdf.Path = field()

    @classmethod
    def from_(cls, prop: Usd.Property) -> PropertyId:
        return PropertyId(
            prim_id=PrimId.from_(prop.GetPrim()),
            path=prop.GetPath(),
        )

    def restore(self, stage: Usd.Stage) -> Usd.Property:
        prim = self.prim_id.restore(stage)
        return prim.GetProperty(self.name)

    @property
    def name(self) -> str:
        return self.path.name

    @property
    def stage_id(self) -> StageId:
        return self.prim_id.stage_id

    def as_str(self) -> str:
        return f"Attribute ({self.name}) {self.prim_id.as_str()}"

    def get_layer_ids(self) -> List[LayerId]:
        return self.prim_id.get_layer_ids()

    def get_spec_ids(self) -> List[SpecId]:
        return [
            SpecId(
                path=node_id.path.AppendProperty(self.name),
                layer_id=node_id.layer_id,
            )
            for node_id in self.prim_id.get_spec_ids()
        ]


class AttributeId(PropertyId):
    """
    A unique identifier of an attribute, i.e. a combination of prim definition and attribute name.
    """


class RelationshipId(PropertyId):
    """
    A unique identifier of an attribute, i.e. a combination of prim definition and attribute name.
    """


def to_identifier(value: Optional[AtType]) -> Optional[Identifier[AtType]]:
    """
    Args:
        value (Optional[AtType]): Optional. An USD object.

    Returns:
        Optional. A identifier (i.e. stateful representation) to a USD object.
    """
    if not value:
        return None
    elif isinstance(value, Usd.Attribute):
        return AttributeId.from_(value)
    elif isinstance(value, Usd.Property):
        return PropertyId.from_(value)
    elif isinstance(value, Usd.Relationship):
        return RelationshipId.from_(value)
    elif isinstance(value, Usd.Prim):
        return PrimId.from_(value)
    elif isinstance(value, Usd.Stage):
        return StageId.from_(value)
    elif isinstance(value, Sdf.Layer):
        return LayerId.from_(value)
    elif isinstance(value, Sdf.PrimSpec):
        return SpecId.from_(value)
    elif isinstance(value, Sdf.PropertySpec):
        return SpecId.from_(value)
    else:
        raise NotImplementedError(f"Unknown type {value}")


def to_identifiers(value: Optional[List[AtType]]) -> Optional[List[Identifier[AtType]]]:
    """
    Iterative version of to_identifier.

    Args:
        value (Optional[List[AtType]]): Optional. A list of USD objects.

    Returns:
        Optional. A list of identifiers (i.e. stateful representation) to a USD object.
    """
    result: List[Identifier[AtType]] = []
    if value is None:
        return None
    for item in value:
        result.append(to_identifier(item))
    return result


# Issue section


class IssueSeverity(Enum):
    """
    Defines the severity of an issue.
    """
    ERROR = 0
    """
    The issue is the result of an actual exception/failure in the code.
    """
    FAILURE = 1
    """
    An indication that it is failing to comply a specific Fule.
    """
    WARNING = 2
    """
    A warning is a suggestion to improve USD, it could be related to performance or memory.
    """
    NONE = 3
    """
    No issue.
    """


@frozen
class Suggestion:
    """
    A suggestion is a combination of a callable and a message describing the suggestion.

    Attributes:
        callable (Callable[[Usd.Stage, AtType], None]): A proposed fix to an issue.
        message (str): A proposed solution to an issue.
        at (Optional[List[Identifier[AtType]]]): Optional. The Layer/SdfLayer/etc... where the issue can be fixed.
    """
    callable: Callable[[Usd.Stage, AtType], None] = field(validator=is_callable(), repr=lambda obj: obj.__name__)
    message: str = field(validator=instance_of(str))
    at: Optional[List[Identifier[AtType]]] = field(default=None, converter=to_identifiers)

    def __contains__(self, item: Identifier[AtType]) -> bool:
        """
        If :py:attr:`Suggestion.at` is provided, check if item is a potential location to fix. If not, it will always
        return True.

        .. code-block:: python

            import omni.asset_validator.core

            suggestion = omni.asset_validator.core.Suggestion(
                callable=lambda stage, at: None,
                message="A suggestion",
                at=Sdf.Layer.FindOrOpen("helloworld.usda"),
            )
            spec_id = omni.asset_validator.core.SpecId(
                layer_id=LayerId(identifier="helloworld.usda"),
                path=Sdf.Path("/World/Cube"),
            )
            print(spec_id in suggestion) # Will print True

            spec_id = omni.asset_validator.core.SpecId(
                layer_id=LayerId(identifier="goodbye.usda"),
                path=Sdf.Path("/World/Cube"),
            )
            print(spec_id in suggestion) # Will print False

        Args:
            item (Identifier[AtType]): An identifier representing a potential location to fix.

        Returns:
            True if the item could be a potential location to fix.
        """
        if self.at is None:
            return True

        lh: Set[LayerId] = set()
        for fix_at in self.at:
            lh.update(fix_at.get_layer_ids())

        rh: Set[LayerId] = set(item.get_layer_ids())
        return len(lh & rh) > 0

    def __call__(self, stage: Usd.Stage, at: AtType) -> None:
        self.callable(stage, at)


@frozen
class Issue:
    """Issues capture information related to Validation Rules:

    Attributes:
        message (str): The reason this issue is mentioned.
        severity (IssueSeverity): The severity associated with the issue.
        rule (Optional[Type[BaseRuleChecker]]): Optional. The class of rule detecting this issue.
        at (Optional[Identifier[AtType]]): Optional. The Prim/Stage/Layer/SdfLayer/SdfPrim/etc.. where this issue arises.
        suggestion (Optional[Suggestion]): Optional. The suggestion to apply. Suggestion
            evaluation (i.e. suggestion()) could raise exception, in which case they will be handled by IssueFixer
            and mark as failed.

    The following exemplifies the expected arguments of an issue:

    .. code-block:: python

        import omni.asset_validator.core

        class MyRule(BaseRuleChecker):
            pass

        stage = Usd.Stage.Open('omniverse://localhost/NVIDIA/Samples/Astronaut/Astronaut.usd')
        prim = stage.GetPrimAtPath("/");

        def my_suggestion(stage: Usd.Stage, at: Usd.Prim):
            pass

        issue = omni.asset_validator.core.Issue(
            message="The reason this issue is mentioned",
            severity=IssueSeverity.ERROR,
            rule=MyRule,
            at=stage,
            suggestion=Suggestion(my_suggestion, "A good suggestion"),
        )

    """
    message: str = field(validator=instance_of(str))
    severity: IssueSeverity = field(validator=instance_of(IssueSeverity), repr=lambda obj: obj.name)
    rule: Optional[RuleType] = field(default=None)
    at: Optional[Identifier[AtType]] = field(default=None, converter=to_identifier)
    suggestion: Optional[Suggestion] = field(default=None, validator=optional(instance_of(Suggestion)))

    @rule.validator
    def _check_rule(self, attribute, value):
        from .complianceChecker import BaseRuleChecker
        if value is None:
            return
        if issubclass(value, BaseRuleChecker):
            return
        raise ValueError("Rule is not of type BaseRuleChecker")

    def __str__(self) -> str:
        """
        String representation of this issue.
        """
        tokens = []
        if self.severity is IssueSeverity.NONE:
            return "No Issue found."
        if self.rule:
            if self.severity is IssueSeverity.ERROR:
                tokens.append(f"Error checking rule {self.rule.__name__}: {self.message}.")
            elif self.severity is IssueSeverity.WARNING:
                tokens.append(f"Warning checking rule {self.rule.__name__}: {self.message}.")
            elif self.severity is IssueSeverity.FAILURE:
                tokens.append(f"Failure checking rule {self.rule.__name__}: {self.message}.")
        else:
            if self.severity is IssueSeverity.ERROR:
                tokens.append(f"Error found: {self.message}.")
            elif self.severity is IssueSeverity.WARNING:
                tokens.append(f"Warning found: {self.message}.")
            elif self.severity is IssueSeverity.FAILURE:
                tokens.append(f"Failure found: {self.message}.")
        if self.at:
            tokens.append(f"At {self.at.as_str()}.")
        if self.suggestion:
            tokens.append(f"Suggestion: {self.suggestion.message}.")
        return " ".join(tokens)

    @classmethod
    def from_message(cls, severity: IssueSeverity, message: str) -> Issue:
        """
        .. deprecated:: 0.4.0 Use Issue constructor.
        """
        return Issue(
            message=message,
            severity=severity,
        )

    @classmethod
    def from_(cls, severity: IssueSeverity, rule: RuleType, message: str) -> Issue:
        """
        .. deprecated:: 0.4.0 Use Issue constructor.
        """
        return Issue(
            message=message,
            severity=severity,
            rule=rule,
        )

    @classmethod
    @lru_cache(maxsize=1)
    def none(cls):
        """
        Returns: Singleton object representing no Issue.
        """
        return Issue(message="No Issue", severity=IssueSeverity.NONE)

    @property
    def all_fix_sites(self) -> List[SpecId]:
        """
        Returns:
            A list of all possible fix sites.
        """
        if self.at is None:
            return []

        spec_ids: List[SpecId] = self.at.get_spec_ids()
        if self.suggestion is None:
            return spec_ids

        included = list(filter(lambda node_id: node_id in self.suggestion, spec_ids))
        excluded = list(filter(lambda node_id: node_id not in self.suggestion, spec_ids))
        return included + excluded

    @property
    def default_fix_site(self) -> Optional[SpecId]:
        """
        Returns:
            The default fix site. The default fix site is generally the Node at root layer.
            Rules can override this behavior by supplying ``Suggestion.at`` locations.
        """
        return next(iter(self.all_fix_sites), None)


IssuePredicate = TypeVar(
    "IssuePredicate",
    bound=Callable[[Issue], bool]
)
"""
An IssuePredicate is a callable that returns True or False for a specific Issue.
"""


class IssuePredicates:
    """
    Convenient methods to filter issues. Additionally, provides :py:meth:`IssuePredicates.And` and
    :py:meth:`IssuePredicates.Or` predicates to chain multiple predicates, see example below.

    .. code-block:: python

        import omni.asset_validator.core

        issues = [
            omni.asset_validator.core.Issue(
                severity=omni.asset_validator.core.IssueSeverity.ERROR, message="This is an error"),
            omni.asset_validator.core.Issue(
                severity=omni.asset_validator.core.IssueSeverity.WARNING, message="Important warning!"),
        ]
        filtered = list(filter(
            omni.asset_validator.core.IssuePredicates.And(
                omni.asset_validator.core.IssuePredicates.IsError(),
                omni.asset_validator.core.IssuePredicates.ContainsMessage("Important"),
            ),
            issues
        ))
    """

    @staticmethod
    def Any() -> IssuePredicate:
        """
        Returns:
            A dummy filter that does not filter.
        """
        return lambda issue: True

    @staticmethod
    def IsFailure() -> IssuePredicate:
        """
        Returns:
            A filter for Issues marked as failure.
        """
        return lambda issue: issue.severity == IssueSeverity.FAILURE

    @staticmethod
    def IsWarning() -> IssuePredicate:
        """
        Returns:
            A filter for Issues marked as warnings.
        """
        return lambda issue: issue.severity == IssueSeverity.WARNING

    @staticmethod
    def IsError() -> IssuePredicate:
        """
        Returns:
            A filter for Issues marked as errors.
        """
        return lambda issue: issue.severity == IssueSeverity.ERROR

    @staticmethod
    def ContainsMessage(text: str) -> IssuePredicate:
        """
        Args:
            text: A specific text to filter the message in issues.
        Returns:
            A filter for messages containing ``text``.
        """
        return lambda issue: text in issue.message

    @staticmethod
    def IsRule(rule: Union[str, RuleType]) -> IssuePredicate:
        return lambda issue: issue.rule.__name__ == rule if isinstance(rule, str) else issue.rule is rule

    @staticmethod
    def HasRootLayer() -> IssuePredicate:
        def wrapper(issue: Issue) -> bool:
            if isinstance(issue.at, (StageId, PrimId, PropertyId)):
                return issue.default_fix_site and issue.default_fix_site.layer_id == issue.at.stage_id.root_layer
            else:
                return False
        return wrapper

    @staticmethod
    def And(lh: IssuePredicate, rh: IssuePredicate) -> IssuePredicate:
        """
        Args:
            lh: First predicate.
            rh: Second predicate.

        Returns:
            A predicate joining two predicates by ``and`` condition.
        """
        return lambda issue: lh(issue) and rh(issue)

    @staticmethod
    def Or(lh: IssuePredicate, rh: IssuePredicate) -> IssuePredicate:
        """
        Args:
            lh: First predicate.
            rh: Second predicate.

        Returns:
            A predicate joining two predicates by ``or`` condition.
        """
        return lambda issue: lh(issue) or rh(issue)



# Fixing Issues.

class FixStatus(Enum):
    """
    Result of fix status.
    """
    NO_LOCATION = 0
    """
    A fix could not be performed as there was no location where to apply a suggestion.
    """

    NO_SUGGESTION = 1
    """
    A fix could not be performed as there was no suggestion to apply.
    """

    FAILURE = 2
    """
    A fix was applied, however it resulted into a failure (i.e. Exception). Check stack trace for more information.
    """

    SUCCESS = 3
    """
    A fix was successfully applied.
    """

    NO_LAYER = 4
    """
    A fix was applied at a specific layer, however the layer is not found in the layer stack.
    """

    INVALID_LOCATION = 5
    """
    A fix could not be performed as the location is no longer valid.
    """


@define
class FixResult:
    """
    FixResult is a combination of input and output to the :py:class:`IssueFixer`.

    Attributes:
        issue (Issue): The issue originating this result. Useful for back tracing.
        status (FixStatus): The status of processing the issue, See `FixStatus`.
        exception (Exception): Optional. If the status is a Failure, it will contain the thrown exception.
    """
    issue: Issue = field(validator=instance_of(Issue))
    status: FixStatus = field(validator=instance_of(FixStatus), repr=lambda obj: obj.name)
    exception: Optional[Exception] = field(default=None, validator=optional(instance_of(Exception)))


def _convert_to_stage(stage: Union[str, Usd.Stage]):
    """
    Args:
        stage (Union[str, Usd.Stage]): Either str or Usd.Stage.
    Returns:
        A Usd.Stage or throws error.
    """
    if not isinstance(stage, (str, Usd.Stage)):
        raise ValueError(f"stage must be of type str or Usd.Stage, {type(stage)}.")
    if isinstance(stage, Usd.Stage):
        return stage
    else:
        return Usd.Stage.Open(Sdf.Layer.FindOrOpen(stage))


@define
class IssueFixer:
    """Fixes issues for the given Asset.

    Attributes:
        asset (Usd.Stage): An in-memory `Usd.Stage`, either provided directly or opened
            from a URI pointing to a Usd layer file.

    .. code-block:: python

        import omni.asset_validator.core

        # validate a layer file
        engine = omni.asset_validator.core.ValidationEngine()
        results = engine.validate('omniverse://localhost/NVIDIA/Samples/Astronaut/Astronaut.usd')
        issues = results.issues()

        # fix that layer file
        fixer = omni.asset_validator.core.IssueFixer('omniverse://localhost/NVIDIA/Samples/Astronaut/Astronaut.usd')
        fixer.fix(issues)
        fixer.save()

        # fix a live stage directly
        stage = Usd.Stage.Open('omniverse://localhost/NVIDIA/Samples/Astronaut/Astronaut.usd')
        engine = omni.asset_validator.core.ValidationEngine()
        results = engine.validate(stage)
        issues = results.issues()

        # fix that same stage in-memory
        fixer = omni.asset_validator.core.IssueFixer(stage)
        fixer.fix(issues)
        fixer.save()

    """
    asset: Usd.Stage = field(converter=_convert_to_stage)
    _layers: Set[Sdf.Layer] = field(init=False, factory=set)

    def apply(self, issue: Issue, at: Optional[Identifier[AtType]] = None) -> FixResult:
        """Fix the specified issues persisting on a specific identifier. If no identifier is given, the default
        fix site is used.

        Args:
            issue (Issue): The issue to fix.
            at (Optional[Identifier[AtType]]): Optional. Apply the changes in a different site.

        Returns:
            An array with the resulting status (i.e. FixResult) of each issue.
        """

        # Validate
        if not issue.at:
            return FixResult(issue, FixStatus.NO_LOCATION)
        if not issue.suggestion:
            return FixResult(issue, FixStatus.NO_SUGGESTION)

        # Restore state
        try:
            obj: AtType = issue.at.restore(self.asset)
        except Exception as error:  # noqa
            return FixResult(issue, FixStatus.INVALID_LOCATION, error)

        # Find edit target
        edit_target: Optional[Usd.EditTarget] = None
        try:
            at = at or issue.default_fix_site
            if at is None:
                edit_target = Usd.EditTarget(self.asset.GetRootLayer())
            elif isinstance(at, Sdf.Layer):
                edit_target = Usd.EditTarget(at)
            elif isinstance(at, SpecId):
                prim_spec: Sdf.Spec = at.restore(self.asset)
                if prim_spec.layer == self.asset.GetRootLayer():
                    edit_target = Usd.EditTarget(self.asset.GetRootLayer())
                else:
                    node_ref: Pcp.NodeRef = obj.GetPrim().GetPrimIndex().GetNodeProvidingSpec(prim_spec)
                    edit_target = Usd.EditTarget(prim_spec.layer, node_ref)
        except Exception as error: # noqa
            return FixResult(issue, FixStatus.INVALID_LOCATION, error)

        if edit_target is None:
            return FixResult(issue, FixStatus.NO_LAYER)
        self._layers.add(edit_target.GetLayer())

        # Apply fix
        with Usd.EditContext(self.asset, edit_target):
            try:
                issue.suggestion(self.asset, obj)
                return FixResult(issue, FixStatus.SUCCESS)
            except Exception as error:  # noqa
                return FixResult(issue, FixStatus.FAILURE, error)

    def fix(self, issues: List[Issue]) -> List[FixResult]:
        """Fix the specified issues in the default layer of each issue.

        Args:
            issues (List[Issue]): The list of issues to fix.

        Returns:
            An array with the resulting status (i.e. FixResult) of each issue.
        """
        return [self.apply(issue, at=None) for issue in issues]

    def fix_at(self, issues: List[Issue], layer: Sdf.Layer) -> List[FixResult]:
        """Fix the specified issues persisting on a provided layer.

        Args:
            issues (List[Issue]): The list of issues to fix.
            layer (Sdf.Layer): Layer where to persist the changes.

        Returns:
            An array with the resulting status (i.e. FixResult) of each issue.
        """
        return [self.apply(issue, at=layer) for issue in issues]

    @property
    def fixed_layers(self) -> List[Sdf.Layer]:
        """
        Returns:
            The layers affected by `fix` or `fix_at` methods.
        """
        return list(self._layers)

    def save(self) -> None:
        """Save the Asset to disk.

        Raises:
            IOError: If writing permissions are not granted.
        """
        # TODO: Add metadata of the changes.
        # Check the logical model.
        for layer in self.fixed_layers:
            if not layer.permissionToEdit:
                raise IOError(f"Cannot edit layer {layer}")
            if not layer.permissionToSave:
                raise IOError(f"Cannot save layer {layer}")
        # Check the physical model.
        for layer in self.fixed_layers:
            result, entry = omni.client.stat(layer.identifier)
            if result != omni.client.Result.OK:
                raise IOError(f"Layer does not exist: {layer}")
            read_only = entry.access & omni.client.AccessFlags.WRITE == 0
            if read_only:
                raise IOError(f"Read only file: {layer}")
        # Save
        self._layers.clear()
        self.asset.Save()

    def backup(self) -> None:
        # TODO: Do backups to nucleus?
        raise NotImplementedError()


@define
class Results:
    """A collection of :py:class:`Issue`.

    Provides convenience mechanisms to filter :py:class:`Issue` by :py:class:`IssuePredicates`.
    """
    _asset: str = field(validator=instance_of(str))
    _issues: List[Issue] = field(validator=deep_iterable(instance_of(Issue)))

    @property
    def asset(self) -> str:
        """
        Returns:
            The Asset corresponding to these Results. Either a path or the result of Usd.Describe.
        """
        return self._asset

    @property
    def errors(self) -> List[str]:
        """
        .. deprecated:: 0.4.0 Use the `issues` method instead.
        """
        return list(map(str, self.issues(IssuePredicates.IsError())))

    @property
    def warnings(self) -> List[str]:
        """
        .. deprecated:: 0.4.0 Use the `issues` method instead.
        """
        return list(map(str, self.issues(IssuePredicates.IsWarning())))

    @property
    def failures(self) -> List[str]:
        """
        .. deprecated:: 0.4.0 Use the `issues` method instead.
        """
        return list(map(str, self.issues(IssuePredicates.IsFailure())))

    def issues(self, predicate: Optional[IssuePredicate] = None) -> List[Issue]:
        """Filter Issues using an option IssuePredicate.

        Args:
            predicate: Optional. A predicate to filter the issues.

        Returns:
            A list of issues matching the predicate or all issues.
        """
        return list(filter(predicate, self._issues))

    def remove_issues(self, predicate: IssuePredicate) -> None:
        """Remove Issues from the Results.

        Convenience to purge issues that can be ignored or have already been processed.

        Args:
            predicate: Optional. A predicate to filter issues.
        """
        # TODO:
        raise NotImplementedError("Not implemented")

    def __str__(self):
        return _dumps(self, indent=0)


# Utilities

def _authoring_layer_property(prop: Usd.Property) -> List[Sdf.Layer]:
    """
    Tries to compute the layers (from stronger to weaker) where the property is authored.

    See Also:
        https://openusd.org/release/glossary.html#propertystack

    Args:
        prop (Usd.Property): A property.

    Returns:
        The layers (from stronger to weaker) where the property is authored.
    """
    layers: List[Sdf.Layer] = []
    for spec in prop.GetPropertyStack(time=Usd.TimeCode.Default()):
        layers.append(spec.layer)
    return layers


def AuthoringLayers(at: Union[AtType, List[AtType]]) -> List[Sdf.Layer]:
    """
    Args:
        at (Union[AtType, List[AtType]]): The location to compute the authoring layers.

    Returns:
        The layers (from stronger to weaker) where the `at` is authored.
    """
    if isinstance(at, list):
        layers = []
        for item in at:
            layers.extend(AuthoringLayers(item))
        return layers
    elif isinstance(at, Usd.Property):
        return _authoring_layer_property(at)
    elif isinstance(at, Usd.Attribute):
        return _authoring_layer_property(at)
    elif isinstance(at, Usd.Relationship):
        return _authoring_layer_property(at)
    else:
        raise NotImplementedError()
