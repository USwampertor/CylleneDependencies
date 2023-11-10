from .engineTests import getUrl
from . import ValidationRuleTestCase, Failure
from omni.asset_validator.core import IssuePredicates


class OmniLayoutRulesTest(ValidationRuleTestCase):
    maxDiff = None

    async def testOmniDefaultPrimChecker(self):
        self.assertRuleFailures(
            url=getUrl("curves.usda"),
            rule="OmniDefaultPrimChecker",
            expectedFailures=[
                Failure("Stage has missing or invalid defaultPrim.*"),
            ],
        )
        self.assertRuleFailures(
            url=getUrl("omniLayoutFailures.usda"),
            rule="OmniDefaultPrimChecker",
            expectedFailures=[
                Failure('Invalid default prim.*not Xformable', at="Prim </World>"),
                Failure('The default prim.*must be active', at="Prim </World>"),
                Failure('Invalid root prim is a sibling.*', at="Prim </Root>"),
            ],
        )

    async def test_activate_prim(self):
        self.assertSuggestion(
            url=getUrl("omniLayoutFailures.usda"),
            rule="OmniDefaultPrimChecker",
            predicate=IssuePredicates.ContainsMessage("The default prim must be active"),
        )

    async def test_deactivate_prim(self):
        self.assertSuggestion(
            url=getUrl("omniLayoutFailures.usda"),
            rule="OmniDefaultPrimChecker",
            predicate=IssuePredicates.ContainsMessage("Invalid root prim is a sibling"),
        )

    async def testOmniOrphanedPrimChecker(self):
        self.assertRuleFailures(
            url=getUrl("omniLayoutFailures.usda"),
            rule="OmniOrphanedPrimChecker",
            expectedFailures=[
                Failure('Prim has an orphaned over.*', at="Prim </Root/Orphan>"),
            ],
        )

        self.assertRuleFailures(
            url=getUrl("validOrphanedOver.usda"),
            rule="OmniOrphanedPrimChecker",
            expectedFailures=[],
        )
