import pathlib, glob
from typing import Optional, Union
from unittest.mock import Mock

from pxr import Usd

import omni.kit.test

import omni.asset_validator.core
from omni.asset_validator.core import IssuePredicates
from .registryTests import MyRuleChecker


def getUrl(relativePath: Optional[Union[str, pathlib.Path]]=""):
    return str(pathlib.Path(__file__).parent.joinpath("data").joinpath(relativePath))


class ValidationEngineTest(omni.kit.test.AsyncTestCase):

    maxDiff = None

    async def testValidLayer(self):
        testFile = getUrl("helloworld.usda")
        result = omni.asset_validator.core.ValidationEngine().validate(testFile)
        self.assertEqual(result.asset, testFile)
        self.assertEqual(result.issues(), [])

    async def testExpectedFailure(self):
        testFile = getUrl("curves.usda")
        result = omni.asset_validator.core.ValidationEngine().validate(testFile)
        self.assertEqual(result.asset, testFile)
        self.assertEqual(result.issues(IssuePredicates.IsError()), [])
        self.assertEqual(result.issues(IssuePredicates.IsWarning()), [])
        self.assertEqual(len(result.issues(IssuePredicates.IsFailure())), 2)
        self.assertTrue(result.issues(
            IssuePredicates.And(IssuePredicates.IsFailure(), IssuePredicates.IsRule("StageMetadataChecker"))))
        self.assertTrue(result.issues(
            IssuePredicates.And(IssuePredicates.IsFailure(), IssuePredicates.IsRule("OmniDefaultPrimChecker"))))

    async def testNonexistantFile(self):
        testFile = getUrl("doesNotExist.usd")
        result = omni.asset_validator.core.ValidationEngine().validate(testFile)
        self.assertEqual(result.asset, testFile)
        self.assertEqual(len(result.issues(IssuePredicates.IsError())), 1)
        self.assertRegexpMatches(result.issues(
            IssuePredicates.IsError())[0].message, 'Accessing.*failed.*ERROR_NOT_FOUND')
        self.assertEqual(result.issues(IssuePredicates.IsWarning()), [])
        self.assertEqual(result.issues(IssuePredicates.IsFailure()), [])

    async def testNonUsdFile(self):
        testFile = getUrl(pathlib.Path("Materials","Fieldstone.mdl"))
        result = omni.asset_validator.core.ValidationEngine().validate(testFile)
        self.assertEqual(result.asset, testFile)
        self.assertEqual(len(result.issues(IssuePredicates.IsError())), 1)
        self.assertRegexpMatches(result.issues(
            IssuePredicates.IsError())[0].message, 'Validation requires a readable USD file.*')
        self.assertEqual(result.issues(IssuePredicates.IsWarning()), [])
        self.assertEqual(result.issues(IssuePredicates.IsFailure()), [])

    async def testConsistentResults(self):
        testFile = getUrl("helloworld.usda")
        engine = omni.asset_validator.core.ValidationEngine()
        result = engine.validate(testFile)
        result2 = engine.validate(testFile)
        self.assertEqual(result, result2)

    async def testEnableCustomRule(self):
        testFile = getUrl("curves.usda")
        engine = omni.asset_validator.core.ValidationEngine()
        engine.enableRule(MyRuleChecker)
        result = engine.validate(testFile)
        self.assertEqual(result.asset, testFile)
        self.assertEqual(result.issues(IssuePredicates.IsError()), [])
        self.assertEqual(result.issues(IssuePredicates.IsWarning()), [])
        self.assertEqual(len(result.issues(IssuePredicates.IsFailure())), 3)
        self.assertTrue(result.issues(
            IssuePredicates.And(IssuePredicates.IsFailure(), IssuePredicates.IsRule("StageMetadataChecker"))))
        self.assertTrue(result.issues(
            IssuePredicates.And(IssuePredicates.IsFailure(), IssuePredicates.IsRule("OmniDefaultPrimChecker"))))
        self.assertTrue(result.issues(
            IssuePredicates.And(IssuePredicates.IsFailure(), IssuePredicates.IsRule("MyRuleChecker"))))

    async def testEnableBuiltinRule(self):
        testFile = getUrl("curves.usda")
        engine = omni.asset_validator.core.ValidationEngine(initRules=False)
        engine.enableRule(omni.asset_validator.core.ValidationRulesRegistry.rule('StageMetadataChecker'))
        result = engine.validate(testFile)
        self.assertEqual(result.asset, testFile)
        self.assertEqual(result.issues(IssuePredicates.IsError()), [])
        self.assertEqual(result.issues(IssuePredicates.IsWarning()), [])
        self.assertEqual(len(result.issues(IssuePredicates.IsFailure())), 1)
        self.assertTrue(result.issues(
            IssuePredicates.And(IssuePredicates.IsFailure(), IssuePredicates.IsRule("StageMetadataChecker"))))

    async def testMinimalRules(self):
        testFile = getUrl("curves.usda")
        engine = omni.asset_validator.core.ValidationEngine(initRules=False)
        engine.enableRule(MyRuleChecker)
        result = engine.validate(testFile)
        self.assertEqual(result.asset, testFile)
        self.assertEqual(result.issues(IssuePredicates.IsError()), [])
        self.assertEqual(result.issues(IssuePredicates.IsWarning()), [])
        self.assertEqual(len(result.issues(IssuePredicates.IsFailure())), 1)
        self.assertTrue(result.issues(
            IssuePredicates.And(IssuePredicates.IsFailure(), IssuePredicates.IsRule("MyRuleChecker"))))

    async def testValidateAsync(self):
        for fileName in (
            "helloworld.usda",
            "curves.usda",
            "doesNotExist.usd",
        ):
            testFile = getUrl(fileName)
            syncResult = omni.asset_validator.core.ValidationEngine().validate(testFile)
            asyncResults = await omni.asset_validator.core.ValidationEngine().validate_async(testFile)
            self.assertEqual(len(asyncResults), 1)
            self.assertEqual(syncResult, asyncResults[0])

    async def testValidateWithCallbacks(self):
        testFile = getUrl("helloworld.usda")

        gather_assets = Mock(spec=omni.asset_validator.core.AssetLocatedCallback)
        assert_results = Mock(spec=omni.asset_validator.core.AssetValidatedCallback)

        await omni.asset_validator.core.ValidationEngine().validate_with_callbacks(
            testFile,
            asset_located_fn=gather_assets,
            asset_validated_fn=assert_results,
        )

        gather_assets.assert_called_with(testFile)
        assert_results.assert_called()
        (async_result, ), _ = assert_results.call_args
        blocking_result = omni.asset_validator.core.ValidationEngine().validate(async_result.asset)
        self.assertEqual(blocking_result, async_result)

    async def testNonexistantFileWithCallbacks(self):
        testFile = getUrl("doesNotExist.usd")

        gather_assets = Mock(spec=omni.asset_validator.core.AssetLocatedCallback)
        assert_results = Mock(spec=omni.asset_validator.core.AssetValidatedCallback)

        await omni.asset_validator.core.ValidationEngine().validate_with_callbacks(
            testFile,
            asset_located_fn=gather_assets,
            asset_validated_fn=assert_results,
        )

        gather_assets.assert_called_with(testFile)
        assert_results.assert_called()
        (async_result, ), _ = assert_results.call_args
        blocking_result = omni.asset_validator.core.ValidationEngine().validate(async_result.asset)
        self.assertEqual(blocking_result, async_result)

    async def testValidateFolderSynchronously(self):
        self.assertRaisesRegex(
            RuntimeError,
            ".*Synchronous validation of folders/containers is not available.*",
            omni.asset_validator.core.ValidationEngine().validate,
            getUrl(),
        )

    async def testValidateFolderAsync(self):
        testDir = getUrl()
        results = await omni.asset_validator.core.ValidationEngine().validate_async(testDir)
        self.assertEqual(
            set(pathlib.Path(x.asset) for x in results),
            set(pathlib.Path(x) for x in glob.glob(getUrl("**/*.usd*"), recursive=True))
        )
        for result in results:
            assetResult = omni.asset_validator.core.ValidationEngine().validate(result.asset)
            self.assertEqual(
                result.issues(IssuePredicates.IsError()),
                assetResult.issues(IssuePredicates.IsError()),
                f'{result.asset} errors did not match'
            )
            self.assertEqual(
                result.issues(IssuePredicates.IsWarning()),
                assetResult.issues(IssuePredicates.IsWarning()),
                f'{result.asset} warnings did not match'
            )
            self.assertEqual(
                len(result.issues(IssuePredicates.IsFailure())),
                len(assetResult.issues(IssuePredicates.IsFailure()))
            )
            for lh, rh in zip(result.issues(IssuePredicates.IsFailure()), assetResult.issues(IssuePredicates.IsFailure())):
                # Some failures produce indeterminate messages (eg anon session layers). Rather than strictly
                # asserting the messages match across validation runs, we just assert that the failures come
                # from the same rules in the same order.
                self.assertEqual(lh.rule, rh.rule, f'{result.asset} failures did not match')

    async def testValidateFolderWithCallbacks(self):

        testDir = getUrl()

        gather_assets = Mock(spec=omni.asset_validator.core.AssetLocatedCallback)
        assert_results = Mock(spec=omni.asset_validator.core.AssetValidatedCallback)

        await omni.asset_validator.core.ValidationEngine().validate_with_callbacks(
            testDir,
            asset_located_fn=gather_assets,
            asset_validated_fn=assert_results,
        )

        gatheredAssets = set()
        for call in gather_assets.call_args_list:
            (asset,), _ = call
            gatheredAssets.add(pathlib.Path(asset))
        self.assertEqual(gatheredAssets, set(pathlib.Path(x) for x in glob.glob(getUrl("**/*.usd*"), recursive=True)))

        for call in assert_results.call_args_list:
            (async_result,), _ = call
            blocking_result = omni.asset_validator.core.ValidationEngine().validate(async_result.asset)
            self.assertEqual(
                blocking_result.issues(IssuePredicates.IsError()),
                async_result.issues(IssuePredicates.IsError()),
                f'{async_result.asset} errors did not match'
            )
            self.assertEqual(
                blocking_result.issues(IssuePredicates.IsWarning()),
                async_result.issues(IssuePredicates.IsWarning()),
                f'{async_result.asset} warnings did not match'
            )
            self.assertEqual(
                len(blocking_result.issues(IssuePredicates.IsFailure())),
                len(async_result.issues(IssuePredicates.IsFailure()))
            )
            for lh, rh in zip(blocking_result.issues(IssuePredicates.IsFailure()), async_result.issues(IssuePredicates.IsFailure())):
                # Some failures produce indeterminate messages (eg anon session layers). Rather than strictly
                # asserting the messages match across validation runs, we just assert that the failures come
                # from the same rules in the same order.
                self.assertEqual(lh.rule, rh.rule, f'{async_result.asset} failures did not match')

    async def testLiveStageSynchronously(self):
        for fileName in (
            "helloworld.usda",
            "curves.usda",
            "omniLayoutFailures.usda",
        ):
            testFile = getUrl(fileName)
            stage = Usd.Stage.Open(testFile)

            syncResult = omni.asset_validator.core.ValidationEngine().validate(testFile)
            liveResults = omni.asset_validator.core.ValidationEngine().validate(stage)
            # the live results will have a stage description rather than the file itself
            self.assertNotEqual(syncResult.asset, liveResults.asset)
            self.assertEqual(syncResult.issues(), liveResults.issues())

    async def testLiveStageAsync(self):
        for fileName in (
            "helloworld.usda",
            "curves.usda",
            "omniLayoutFailures.usda",
        ):
            testFile = getUrl(fileName)
            stage = Usd.Stage.Open(testFile)
            layers = [ x.identifier for x in stage.GetLayerStack() ]
            target = stage.GetEditTarget()
            syncResult = omni.asset_validator.core.ValidationEngine().validate(testFile)
            liveResults = await omni.asset_validator.core.ValidationEngine().validate_async(stage)
            self.assertEqual(len(liveResults), 1)
            # the live results will have a stage description rather than the file itself
            self.assertNotEqual(syncResult.asset, liveResults[0].asset)
            self.assertEqual(syncResult.issues(), liveResults[0].issues())
            # makes sure the stage's edit target and layer stack are preserved
            # we should probably test that composition is unchanged, but we can
            # leave that until it comes up as an issue.
            self.assertEqual(stage.GetEditTarget(), target)
            self.assertEqual(len(stage.GetLayerStack()), len(layers))
            for i in range(len(layers)):
                self.assertEqual(stage.GetLayerStack()[i].identifier, layers[i])

    async def testLiveStageAsyncWithCallbacks(self):
        testFile = getUrl("helloworld.usda")
        stage = Usd.Stage.Open(testFile)

        gather_assets = Mock(spec=omni.asset_validator.core.AssetLocatedCallback)
        assert_results = Mock(spec=omni.asset_validator.core.AssetValidatedCallback)

        await omni.asset_validator.core.ValidationEngine().validate_with_callbacks(
            stage,
            asset_located_fn=gather_assets,
            asset_validated_fn=assert_results,
        )

        gather_assets.assert_called_with(stage)
        assert_results.assert_called()
        (async_result, ), _ = assert_results.call_args
        self.assertEqual(async_result.asset, omni.asset_validator.core.ValidationEngine.describe(stage))
        blocking_result = omni.asset_validator.core.ValidationEngine().validate(testFile)
        # the live results will have a stage description rather than the file itself
        self.assertNotEqual(blocking_result.asset, async_result.asset)
        self.assertEqual(blocking_result.issues(), async_result.issues())

    async def testFilteredValidation(self):
        testFile = getUrl("basicFailures.usda")
        stage = Usd.Stage.Open(testFile)
        fullResult = omni.asset_validator.core.ValidationEngine().validate(stage)

        mask = Usd.StagePopulationMask()
        mask.Add('/Root/Sphere')
        mask.Add('/NotAModel/StillNotAModel/BaseKind')
        mask.Add('/Root/Looks/Surface')
        stage.SetPopulationMask(mask)
        maskedResult = omni.asset_validator.core.ValidationEngine().validate(stage)

        self.assertEqual(fullResult.asset, maskedResult.asset)
        self.assertEqual(fullResult.issues(IssuePredicates.IsError()), maskedResult.issues(IssuePredicates.IsError()))
        self.assertEqual(fullResult.issues(IssuePredicates.IsWarning()), maskedResult.issues(IssuePredicates.IsWarning()))
        self.assertTrue(len(fullResult.issues(IssuePredicates.IsFailure())) > len(maskedResult.issues(IssuePredicates.IsFailure())))
        self.assertNotEqual(fullResult.issues(IssuePredicates.IsFailure()), maskedResult.issues(IssuePredicates.IsFailure()))

        for failure in maskedResult.issues(IssuePredicates.IsFailure()):
            self.assertTrue(failure in fullResult.issues(IssuePredicates.IsFailure()))
