import os.path
import pathlib
import unittest

from .engineTests import getUrl
from . import ValidationRuleTestCase, Failure
from omni.asset_validator.core import IssuePredicates

class UsdGeomSubsetCheckerTest(ValidationRuleTestCase):
    async def test_attributes(self):
        self.assertRuleFailures(
            url=getUrl("usdGeomSubset.usda"),
            rule="UsdGeomSubsetChecker",
            expectedFailures=[
                Failure("GeomSubset 'subset' has a material binding but no valid family name attribute.", at="Prim </World/Cube/subset>"),
                Failure("GeomSubset 'subset2' has a material binding but no valid family name attribute.", at="Prim </World/Cube2/subset2>"),
            ],
        )

    async def test_autofix_suggestions(self):
        self.assertSuggestion(
            url=getUrl("usdGeomSubset.usda"),
            rule="UsdGeomSubsetChecker",
            predicate=IssuePredicates.ContainsMessage("GeomSubset")
        )
