from typing import List, Union, Optional

import omni.kit.test

from omni.asset_validator.core import (
    BaseRuleChecker,
    ValidationEngine,
    ValidationRulesRegistry,
    Issue,
    IssuePredicates,
    IssueFixer,
    IssuePredicate,
    FixStatus,
)

from attr import define, field
from attr.validators import optional, instance_of
from pxr import Usd, Sdf

@define
class Failure:
    """
    Failure let us assert messages and locations of failure to match against the real issue found in Validation
    Engine. This class is used in conjuntion to ValidationRuleTestCase.assertRuleFailures.

    Attributes:
        message: Regex pattern for the expected failure message.
        at: Optional. The expected location of the expected failure.
    """
    message: str = field(validator=instance_of(str))
    at: Optional[str] = field(default=None, validator=optional(instance_of(str)))

    def test(self, test: omni.kit.test.AsyncTestCase, issue: Issue) -> None:
        if self.message != issue.message:
            test.assertRegex(issue.message, self.message)
        if self.at is not None:
            test.assertEquals(issue.at.as_str(), self.at)


class ValidationRuleTestCase(omni.kit.test.AsyncTestCase):
    """A base class to simplify testing of individual Validation Rules

    Rule authors can derive from this class and use the assertions to test the rule produces the expected results.

    Example:
        Define a new Rule, register it, and define a new TestCase class to exercise it.

        .. code-block:: python

            import omni.asset_validator.core

            class MyRuleChecker(omni.asset_validator.core.BaseRuleChecker):
                '''
                Check that all prims are meshes for xforms
                '''

                def CheckPrim(self, prim) -> None:
                    self._Msg("Checking prim <%s>." % prim.GetPath())
                    if prim.GetTypeName() not in ("Mesh", "Xform"):
                        self._AddFailedCheck(
                            f"Prim <{prim.GetPath()}> has unsupported type '{prim.GetTypeName()}'."
                        )

            omni.asset_validator.core.ValidationRulesRegistry.registerRule(MyRuleChecker, "MyOwnRules")

            import omni.asset_validator.core.tests

            class MyTestCase(omni.asset_validator.core.tests.ValidationRuleTestCase):
                async def testMyRuleChecker(self):
                    self.assertRuleFailures(
                        url='omniverse://localhost/NVIDIA/Samples/Astronaut/Astronaut.usd',
                        rule=MyRuleChecker,
                        expectedFailures=[
                            Failure("Prim.*has unsupported type.*"),
                        ],
                    )

            # run test case via an appropriate TestSuite
    """

    def assertRuleFailures(
        self,
        url: str,
        rule: Union[BaseRuleChecker, str],
        expectedFailures: List[Failure]
    ) -> None:
        """Assert expected failures from validating one asset using one rule

        Derived classes may use this to simplify testing of new rules with less consideration for
        the structure of `omni.asset_validator.core.Results`.

        Note there will be only one enabled rule for the validation run, so all results will have necessarily
        been produced by the provided rule or by the engine itself (eg non-existent file).

        Args:
            url: A single asset to validate
            rule: Either a BaseRuleChecker derived class or the str class name of such a class
            expectedFailures: A list of failures (or empty list if it should succeed).
        """
        engine = omni.asset_validator.core.ValidationEngine(initRules=False)
        if isinstance(rule, str):
            rule = ValidationRulesRegistry.rule(rule)
        engine.enableRule(rule)
        result = engine.validate(url)

        self.assertEqual(result.asset, url)
        self.assertEqual(result.errors, [])
        self.assertEqual(result.warnings, [])

        failures = result.issues(IssuePredicates.IsFailure())
        self.assertEqual(
            len(failures), len(expectedFailures),
            'Unexpected number of failures:\n{actual}\nvs\n{expected}'.format(
                actual="\n".join(map(repr, failures)),
                expected="\n".join(map(repr, expectedFailures)),
            )
        )
        for expectedFailure, failure in zip(expectedFailures, failures):
            expectedFailure.test(self, failure)

    def assertSuggestion(
        self,
        url: str,
        rule: Union[BaseRuleChecker, str],
        predicate: Optional[IssuePredicate],
    ) -> None:
        """Assert expected failures from validating one asset using one rule will be fixed using auto fix framework.

        Derived classes may use this to simplify testing of new rules with less consideration for
        the structure of `omni.asset_validator.core.IssueFixer`.

        Note there will be only one enabled rule for the validation run, so all results will have necessarily
        been produced by the provided rule or by the engine itself (eg non-existent file).

        Args:
            url: A single asset to validate
            rule: Either a BaseRuleChecker derived class or the str class name of such a class
            predicate: A predicate (i.e. Callable[[Issue], bool]) to filter out issues.
        """
        engine = ValidationEngine(initRules=False)
        if isinstance(rule, str):
            rule = ValidationRulesRegistry.rule(rule)
        engine.enableRule(rule)

        # Reuse single stage
        stage: Usd.Stage = Usd.Stage.Open(url)
        # Perform changes in specific layer to drop later
        session_layer: Sdf.Layer = Sdf.Layer.CreateAnonymous()
        stage.GetSessionLayer().subLayerPaths.append(session_layer.identifier)

        result = engine.validate(stage)
        issues = result.issues(predicate)
        self.assertTrue(issues)

        # Perform fixing
        fixer = omni.asset_validator.core.IssueFixer(stage)
        results = fixer.fix_at(issues, session_layer)
        for result in results:
            if result.status is FixStatus.FAILURE:
                raise result.exception

        # Assert issues are gone
        result = engine.validate(stage)
        issues = result.issues(predicate)
        self.assertFalse(issues)
