import os.path
import pathlib
import unittest

from .engineTests import getUrl
from . import ValidationRuleTestCase, Failure
from omni.asset_validator.core import IssuePredicates

class UsdDanglingMaterialBindingChecker(ValidationRuleTestCase):
    async def test_api(self):
        self.assertRuleFailures(
            url=getUrl("usdDanglingMaterialBindingFail.usda"),
            rule="UsdDanglingMaterialBinding",
            expectedFailures=[
                Failure("Prim 'Mesh' has a material binding to '/Material' but that location does not exist", at="Prim </World/Mesh>"),
            ],
        )

    async def test_pass(self):
        self.assertRuleFailures(
            url=getUrl("usdDanglingMaterialBindingPass.usda"),
            rule="UsdDanglingMaterialBinding",
            expectedFailures=[],
        )

    async def test_autofix_suggestions(self):
        self.assertSuggestion(
            url=getUrl("usdDanglingMaterialBindingFail.usda"),
            rule="UsdDanglingMaterialBinding",
            predicate=IssuePredicates.ContainsMessage("Prim 'Mesh' has a material binding to '/Material'")
        )
