import pathlib
from contextlib import contextmanager
from typing import List
from unittest.mock import Mock, patch

import omni.kit.test

from omni.asset_validator.core import (
    AuthoringLayers,
    Issue,
    IssueSeverity,
    IssueFixer,
    FixStatus,
    BaseRuleChecker,
    Identifier,
    to_identifier,
    ValidationEngine,
    IssuePredicates,
    Results,
    FixResult,
    SpecId,
    SpecIdList,
    Suggestion,
)
from pxr import Usd, Sdf, Pcp
from .engineTests import getUrl


class MyRuleChecker(BaseRuleChecker):
    """
    Check that all active prims are meshes for xforms
    """

    def CheckPrim(self, prim) -> None:
        self._Msg("Checking prim <%s>." % prim.GetPath())
        if prim.GetTypeName() not in ("Mesh", "Xform") and prim.IsActive():
            self._AddFailedCheck(
                message=f"Prim <{prim.GetPath()}> has unsupported type '{prim.GetTypeName()}'.",
                at=prim,
                suggestion=Suggestion(
                    lambda _, _prim: _prim.SetActive(False),
                    "Disable Prim",
                )
            )


class IdentifierTests(omni.kit.test.AsyncTestCase):

    async def test_stage_id(self):
        # Given / When
        def _create_identifier() -> Identifier[Usd.Stage]:
            stage = Usd.Stage.Open(getUrl("helloworld.usda"))
            return to_identifier(stage)

        # Then
        stage_id = _create_identifier()
        stage = Usd.Stage.Open(getUrl("helloworld.usda"))
        self.assertTrue(stage is stage_id.restore(stage))

    async def test_prim_id(self):
        # Given / When
        def _create_identifier() -> Identifier[Usd.Prim]:
            stage = Usd.Stage.Open(getUrl("helloworld.usda"))
            prim = stage.GetPrimAtPath("/World/cube")
            return to_identifier(prim)

        # Then
        prim_id = _create_identifier()
        stage = Usd.Stage.Open(getUrl("helloworld.usda"))
        prim = stage.GetPrimAtPath("/World/cube")
        self.assertTrue(prim == prim_id.restore(stage))

    async def test_spec_id(self):
        # Given / When
        def _create_identifier() -> Identifier[Usd.Prim]:
            stage = Usd.Stage.Open(getUrl("helloworld.usda"))
            prim = stage.GetPrimAtPath("/World/cube")
            return to_identifier(prim)

        # Then
        prim_id = _create_identifier()
        self.assertEqual(prim_id.spec_ids.root_path, "/World/cube")
        self.assertEqual(prim_id.spec_ids.spec_ids[0].path, "/World/cube")
        self.assertEqual(pathlib.Path(prim_id.spec_ids.spec_ids[0].layer_id.identifier),
                         pathlib.Path(getUrl("helloworld.usda")))

    async def test_spec_id_anonymous(self):
        # OM-94661: Test for filtering out anonymous layers. Anonymous layers are temporary.
        # Given
        node: Sdf.PrimSpec = Mock(spec=Sdf.PrimSpec)
        node.path = "/World"
        node.layer = Sdf.Layer.FindOrOpen(getUrl("helloworld.usda"))

        other: Sdf.PrimSpec = Mock(spec=Sdf.PrimSpec)
        other.path = "/World"
        other.layer = Sdf.Layer.CreateAnonymous("Checker")

        prim: Usd.Prim = Mock(spec=Usd.Prim)
        prim.GetPrimPath.return_value = node.path
        prim.GetPrimStack.return_value = [other, node]

        # When
        result: SpecIdList = SpecIdList.from_(prim)

        # Then
        self.assertEqual(result.root_path, "/World")
        self.assertEqual(len(result.spec_ids), 1)
        self.assertEqual(result.spec_ids[0].path, "/World")
        self.assertEqual(pathlib.Path(result.spec_ids[0].layer_id.identifier),
                         pathlib.Path(getUrl("helloworld.usda")))

    async def test_attribute_id(self):
        # Given / When
        def _create_identifier() -> Identifier[Usd.Attribute]:
            stage = Usd.Stage.Open(getUrl("helloworld.usda"))
            prim = stage.GetPrimAtPath("/World/cube")
            attr = prim.GetAttribute("size")
            return to_identifier(attr)

        # Then
        attr_ref = _create_identifier()
        stage = Usd.Stage.Open(getUrl("helloworld.usda"))
        prim = stage.GetPrimAtPath("/World/cube")
        attr = prim.GetAttribute("size")
        self.assertTrue(attr == attr_ref.restore(stage))


class IssueTests(omni.kit.test.AsyncTestCase):

    async def setUp(self):
        super().setUp()
        self.layer = Mock(spec=Sdf.Layer)
        self.layer.anonymous = False
        self.layer.identifier = getUrl("helloworld.usda")
        self.stage = Mock(spec=Usd.Stage)
        self.stage.GetRootLayer.return_value = self.layer
        self.node = Mock(spec=Sdf.PrimSpec)
        self.node.path = "/World"
        self.node.layer = self.layer
        self.prim = Mock(spec=Usd.Prim)
        self.prim.GetStage.return_value = self.stage
        self.prim.GetPrimPath.return_value = self.node.path
        self.prim.GetPrimStack.return_value = [self.node]

    async def test_issue_construct_rule(self):
        # Given
        class Rule:
            pass

        # When / Then
        with self.assertRaises(ValueError):
            Issue(message="message", severity=IssueSeverity.ERROR, rule=Rule)

    async def test_issue_none_str(self):
        # Given / When
        issue = Issue.none()
        # Then
        self.assertEquals(str(issue), "No Issue found.")

    async def test_issue_error_str(self):
        # Given / When
        issue = Issue(
            message="A message",
            severity=IssueSeverity.ERROR,
            rule=MyRuleChecker,
        )
        # Then
        self.assertEquals(str(issue), "Error checking rule MyRuleChecker: A message.")

    async def test_issue_error_str_at_fix(self):
        # Given
        at = self.prim
        suggestion = Suggestion(message="suggestion", callable=lambda a, b: None)
        # When
        issue = Issue(
            message="A message",
            severity=IssueSeverity.ERROR,
            rule=MyRuleChecker,
            at=at,
            suggestion=suggestion,
        )
        # Then
        self.assertEquals(
            str(issue),
            "Error checking rule MyRuleChecker: A message. At Prim </World>. Suggestion: suggestion."
        )

    async def test_issue_warning_str(self):
        # Given / When
        issue = Issue(
            message="A message",
            severity=IssueSeverity.WARNING,
            rule=MyRuleChecker,
        )
        # Then
        self.assertEquals(str(issue), "Warning checking rule MyRuleChecker: A message.")

    async def test_issue_warning_str_at_fix(self):
        # Given
        at = self.prim
        suggestion = Suggestion(message="suggestion", callable=lambda a, b: None)
        # When
        issue = Issue(
            message="A message",
            severity=IssueSeverity.WARNING,
            rule=MyRuleChecker,
            at=at,
            suggestion=suggestion,
        )
        # Then
        self.assertEquals(
            str(issue),
            "Warning checking rule MyRuleChecker: A message. At Prim </World>. Suggestion: suggestion."
        )

    async def test_issue_failure_str(self):
        # Given / When
        issue = Issue(
            message="A message",
            severity=IssueSeverity.FAILURE,
            rule=MyRuleChecker,
        )
        # Then
        self.assertEquals(str(issue), "Failure checking rule MyRuleChecker: A message.")

    async def test_issue_failure_str_at_fix(self):
        # Given
        at = self.prim
        suggestion = Suggestion(message="suggestion", callable=lambda a, b: None)
        # When
        issue = Issue(
            message="A message",
            severity=IssueSeverity.FAILURE,
            rule=MyRuleChecker,
            at=at,
            suggestion=suggestion,
        )
        # Then
        self.assertEquals(
            str(issue),
            "Failure checking rule MyRuleChecker: A message. At Prim </World>. Suggestion: suggestion."
        )


class IssueFixerTests(omni.kit.test.AsyncTestCase):

    async def setUp(self) -> None:
        super().setUp()
        self.layer = Mock(spec=Sdf.Layer)
        self.layer.anonymous = False
        self.layer.identifier = getUrl("helloworld.usda")
        self.stage = Mock(spec=Usd.Stage)
        self.stage.GetRootLayer.return_value = self.layer
        self.edit_target_patch = patch("pxr.Usd.EditTarget")
        self.edit_target = self.edit_target_patch.start()
        self.edit_target.return_value.GetLayer.return_value = self.layer
        self.edit_context_patch = patch("pxr.Usd.EditContext")
        self.edit_context = self.edit_context_patch.start()
        self.issue = Mock(spec=Issue)
        self.issue.at = Mock(spec=Identifier)
        self.issue.default_fix_site = None

    async def tearDown(self) -> None:
        super().tearDown()
        self.edit_target_patch.stop()
        self.edit_context_patch.stop()

    async def test_init_fails(self):
        # Given
        prim = Mock(spec=Usd.Prim)
        # When / Then
        with self.assertRaises(ValueError):
            IssueFixer(prim)

    async def test_apply_success(self):
        # Given / When
        fixer = IssueFixer(self.stage)
        status = fixer.apply(self.issue)
        fixer.save()
        # Then
        self.assertEquals(status, FixResult(self.issue, FixStatus.SUCCESS))
        self.edit_target.asset_called_with(self.stage.GetRootLayer())
        self.issue.suggestion.assert_called_with(self.stage, self.issue.at.restore.return_value)
        self.stage.Save.assert_called_with()

    async def test_apply_no_location(self):
        # Given
        self.issue.at = None
        # When
        fixer = IssueFixer(self.stage)
        status = fixer.apply(self.issue)
        fixer.save()
        # Then
        self.assertEquals(status, FixResult(self.issue, FixStatus.NO_LOCATION))
        self.edit_target.assert_not_called()
        self.issue.suggestion.assert_not_called()
        self.stage.Save.assert_called_with()

    async def test_apply_no_suggestion(self):
        # Given
        self.issue.suggestion = None
        # When
        fixer = IssueFixer(self.stage)
        status = fixer.apply(self.issue)
        fixer.save()
        # Then
        self.assertEquals(status, FixResult(self.issue, FixStatus.NO_SUGGESTION))
        self.edit_target.assert_not_called()
        self.stage.Save.assert_called_with()

    async def test_apply_location_error(self):
        # Given
        exception = ValueError()
        self.issue.at.restore.side_effect = exception
        # When
        fixer = IssueFixer(self.stage)
        status = fixer.apply(self.issue)
        fixer.save()
        # Then
        self.assertEquals(status, FixResult(self.issue, FixStatus.INVALID_LOCATION, exception))
        self.edit_target.assert_not_called()
        self.issue.suggestion.assert_not_called()
        self.stage.Save.assert_called_with()

    async def test_apply_at_error(self):
        # Given
        exception = ValueError()
        at: SpecId = Mock(spec=SpecId)
        at.restore.side_effect = exception
        # Given / When
        fixer = IssueFixer(self.stage)
        status = fixer.apply(self.issue, at)
        fixer.save()
        # Then
        self.assertEquals(status, FixResult(self.issue, FixStatus.INVALID_LOCATION, exception))
        self.edit_target.assert_not_called()
        self.issue.suggestion.assert_not_called()
        self.stage.Save.assert_called_with()

    async def test_apply_failure(self):
        # Given
        exception = ValueError()
        self.issue.suggestion.side_effect = exception
        # When
        fixer = IssueFixer(self.stage)
        status = fixer.apply(self.issue)
        fixer.save()
        # Then
        self.assertEquals(status, FixResult(self.issue, FixStatus.FAILURE, exception))
        self.edit_target.asset_called_with(self.stage.GetRootLayer())
        self.issue.suggestion.assert_called_with(self.stage, self.issue.at.restore.return_value)
        self.stage.Save.assert_called_with()

    async def test_apply_layer_success(self):
        # Given / When
        fixer = IssueFixer(self.stage)
        status = fixer.apply(self.issue, self.layer)
        fixer.save()
        # Then
        self.assertEquals(status, FixResult(self.issue, FixStatus.SUCCESS))
        self.edit_target.asset_called_with(self.layer)
        self.issue.suggestion.assert_called_with(self.stage, self.issue.at.restore.return_value)
        self.stage.Save.assert_called_with()

    async def test_apply_layer_failure(self):
        # Given
        exception = ValueError()
        self.issue.suggestion.side_effect = exception
        # When
        fixer = IssueFixer(self.stage)
        status = fixer.apply(self.issue, self.layer)
        fixer.save()
        # Then
        self.assertEquals(status, FixResult(self.issue, FixStatus.FAILURE, exception))
        self.edit_target.asset_called_with(self.layer)
        self.issue.suggestion.assert_called_with(self.stage, self.issue.at.restore.return_value)
        self.stage.Save.assert_called_with()

    async def test_fix_success(self):
        # Given / When
        fixer = IssueFixer(self.stage)
        status = fixer.fix([self.issue])
        fixer.save()
        # Then
        self.assertEquals(status, [FixResult(self.issue, FixStatus.SUCCESS)])
        self.issue.suggestion.assert_called_with(self.stage, self.issue.at.restore.return_value)
        self.stage.Save.assert_called_with()

    async def test_fix_failure(self):
        # Given
        exception = ValueError()
        self.issue.suggestion.side_effect = exception
        # When
        fixer = IssueFixer(self.stage)
        status = fixer.fix([self.issue])
        fixer.save()
        # Then
        self.assertEquals(status, [FixResult(self.issue, FixStatus.FAILURE, exception)])
        self.issue.suggestion.assert_called_with(self.stage, self.issue.at.restore.return_value)
        self.stage.Save.assert_called_with()

    async def test_fix_io_error(self):
        # Given
        self.layer.identifier = "notfound.usda"
        # When
        fixer = IssueFixer(self.stage)
        status = fixer.fix([self.issue])
        with self.assertRaises(IOError):
            fixer.save()
        # Then
        self.assertEquals(status, [FixResult(self.issue, FixStatus.SUCCESS)])
        self.issue.suggestion.assert_called_with(self.stage, self.issue.at.restore.return_value)
        self.stage.Save.assert_not_called()

    async def test_fix_at(self):
        # Given
        stage = Usd.Stage.Open(getUrl("helloworld.usda"))
        session_layer: Sdf.Layer = Sdf.Layer.CreateAnonymous()
        stage.GetSessionLayer().subLayerPaths.append(session_layer.identifier)
        self.edit_target.return_value.GetLayer.return_value = session_layer
        # When
        fixer = IssueFixer(stage)
        status = fixer.fix_at([self.issue], session_layer)
        with self.assertRaises(IOError):
            fixer.save()
        # Then
        self.assertEquals(status, [FixResult(self.issue, FixStatus.SUCCESS)])
        self.issue.suggestion.assert_called_with(stage, self.issue.at.restore.return_value)
        self.stage.Save.assert_not_called()


class BaseRuleCheckerTest(omni.kit.test.AsyncTestCase):
    """
    Tests modifications to BaseRuleChecker.
    """

    async def setUp(self) -> None:
        super().setUp()
        self.message = "A message"
        self.suggestion = Mock(spec=Suggestion)
        self.layer = Mock(spec=Sdf.Layer)
        self.layer.anonymous = False
        self.layer.identifier = getUrl("helloworld.usda")
        self.stage = Mock(spec=Usd.Stage)
        self.stage.GetRootLayer.return_value = self.layer
        self.node = Mock(spec=Sdf.PrimSpec)
        self.node.path = "/World/cube"
        self.node.layer = self.layer
        self.prim = Mock(spec=Usd.Prim)
        self.prim.GetStage.return_value = self.stage
        self.prim.GetPrimPath.return_value = self.node.path
        self.prim.GetPrimStack.return_value = [self.node]

    async def test_add_failed_check_legacy(self):
        # Given / When
        checker = MyRuleChecker(False, False, False)
        checker._AddFailedCheck(
            message=self.message,
        )
        # Then
        self.assertEquals(checker.GetFailedChecks(),
                          [Issue(severity=IssueSeverity.FAILURE, rule=MyRuleChecker, message=self.message)])

    async def test_add_error_legacy(self):
        # Given / When
        checker = MyRuleChecker(False, False, False)
        checker._AddError(
            message=self.message,
        )
        # Then
        self.assertEquals(checker.GetErrors(),
                          [Issue(severity=IssueSeverity.ERROR, rule=MyRuleChecker, message=self.message)])

    async def test_add_warning_legacy(self):
        # Given / When
        checker = MyRuleChecker(False, False, False)
        checker._AddWarning(
            message=self.message,
        )
        # Then
        self.assertEquals(checker.GetWarnings(),
                          [Issue(severity=IssueSeverity.WARNING, rule=MyRuleChecker, message=self.message)])

    async def test_add_failed_check(self):
        # Given / When
        checker = MyRuleChecker(False, False, False)
        checker._AddFailedCheck(
            message=self.message,
            at=self.prim,
            suggestion=self.suggestion,
        )
        # Then
        self.assertEquals(checker.GetFailedChecks(),
                          [omni.asset_validator.core.Issue(
                              message=self.message,
                              severity=IssueSeverity.FAILURE,
                              rule=MyRuleChecker,
                              at=self.prim,
                              suggestion=self.suggestion,
                          )])

    async def test_add_error_check(self):
        # Given / When
        checker = MyRuleChecker(False, False, False)
        checker._AddError(
            message=self.message,
            at=self.prim,
            suggestion=self.suggestion,
        )
        # Then
        self.assertEquals(checker.GetErrors(),
                          [omni.asset_validator.core.Issue(
                              message=self.message,
                              severity=IssueSeverity.ERROR,
                              rule=MyRuleChecker,
                              at=self.prim,
                              suggestion=self.suggestion,
                          )])

    async def test_add_warning_check(self):
        # Given / When
        checker = MyRuleChecker(False, False, False)
        checker._AddWarning(
            message=self.message,
            at=self.prim,
            suggestion=self.suggestion,
        )
        # Then
        self.assertEquals(checker.GetWarnings(),
                          [omni.asset_validator.core.Issue(
                              message=self.message,
                              severity=IssueSeverity.WARNING,
                              rule=MyRuleChecker,
                              at=self.prim,
                              suggestion=self.suggestion,
                          )])


class ResultsTest(omni.kit.test.AsyncTestCase):
    def test_str(self):
        # Given
        result = Results(
            asset="Any asset",
            issues=[Issue(severity=IssueSeverity.FAILURE, rule=MyRuleChecker, message="failure")]
        )
        # When
        actual = str(result)
        # Then
        self.assertEquals(
            actual,
            """Results(
    asset="Any asset",
    issues=[
        Issue(
            message="failure",
            severity=IssueSeverity.FAILURE,
            rule=MyRuleChecker,
            at=None,
            suggestion=None
        )
    ]
)"""
        )


class AutoFixTest(omni.kit.test.AsyncTestCase):
    """
    Showcases of auto fix.
    """

    @contextmanager
    def rollback(self, path) -> pathlib.Path:
        file = pathlib.Path(path)
        old_content = file.read_text()
        try:
            yield file
        finally:
            file.write_text(old_content)

    async def test_commit_fix_url(self):
        url: str = getUrl("autofixPrev.usda")
        engine = ValidationEngine(initRules=False)
        engine.enableRule(MyRuleChecker)

        result = engine.validate(url)
        issues = result.issues(
            IssuePredicates.And(
                IssuePredicates.IsFailure(),
                IssuePredicates.IsRule(MyRuleChecker)
            )
        )

        with self.rollback(url) as path:
            fixer = IssueFixer(url)
            fixer.fix(issues)
            fixer.save()
            self.assertEquals(
                path.read_text(),
                pathlib.Path(getUrl("autofixNext.usda")).read_text()
            )


class UtilsTest(omni.kit.test.AsyncTestCase):

    def test_authoring_layer(self):
        stage: Usd.Stage = Usd.Stage.Open(getUrl("materialLayerKnownReference.usda"))
        prim: Usd.Prim = stage.GetPrimAtPath("/World/Looks/MatX/Shader")
        prop: Usd.Property = prim.GetProperty("info:mdl:sourceAsset")

        result: List[Sdf.Layer] = AuthoringLayers(prop)

        self.assertTrue(result)
        self.assertTrue(result[0].identifier.endswith("knownMaterial.usda"))
