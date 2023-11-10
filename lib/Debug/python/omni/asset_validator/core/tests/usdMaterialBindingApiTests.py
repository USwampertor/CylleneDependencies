import os.path
import pathlib
import unittest

from .engineTests import getUrl
from . import ValidationRuleTestCase, Failure
from omni.asset_validator.core import IssuePredicates

class UsdMaterialBindingApiChecker(ValidationRuleTestCase):
    async def test_api(self):
        self.assertRuleFailures(
            url=getUrl("usdMaterialBindingApiFail.usda"),
            rule="UsdMaterialBindingApi",
            expectedFailures=[
                Failure("Prim 'Mesh' has a material binding but does not have the MaterialBindingApi.", at="Prim </World/Mesh>"),
            ],
        )

    async def test_pass(self):
        self.assertRuleFailures(
            url=getUrl("usdMaterialBindingApiPass.usda"),
            rule="UsdMaterialBindingApi",
            expectedFailures=[],
        )

    async def test_autofix_suggestions(self):
        self.assertSuggestion(
            url=getUrl("usdMaterialBindingApiFail.usda"),
            rule="UsdMaterialBindingApi",
            predicate=IssuePredicates.ContainsMessage("MaterialBindingApi")
        )
