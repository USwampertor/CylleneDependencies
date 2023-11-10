import os.path
import pathlib
import unittest

from .engineTests import getUrl
from . import ValidationRuleTestCase, Failure
from omni.asset_validator.core import IssuePredicates


class OmniMaterialPathCheckerTest(ValidationRuleTestCase):

    # No references

    async def test_relative_known_path(self):
        self.assertRuleFailures(
            url=getUrl("Materials/knownMaterial.usda"),
            rule="OmniMaterialPathChecker",
            expectedFailures=[],
        )

    async def test_relative_unknown_path(self):
        self.assertRuleFailures(
            url=getUrl("Materials/unknownMaterial.usda"),
            rule="OmniMaterialPathChecker",
            expectedFailures=[
                Failure("The relative path ./unknown.mdl does not exist.",
                        at="Attribute (info:mdl:sourceAsset) Prim </Looks/MatX/Shader>"),
            ]
        )

    async def test_missing_dot_slash_relative_known_path(self):
        self.assertRuleFailures(
            url=getUrl("Materials/incorrectMaterial.usda"),
            rule="OmniMaterialPathChecker",
            expectedFailures=[
                Failure("Relative path material.mdl should be corrected to ./material.mdl.",
                        at="Attribute (info:mdl:sourceAsset) Prim </Looks/MatX/Shader>"),
            ],
        )

    async def test_missing_dot_relative_known_path(self):
        self.assertRuleFailures(
            url=getUrl("Materials/missingDotMaterial.usda"),
            rule="OmniMaterialPathChecker",
            expectedFailures=[
                Failure("Relative path /material.mdl should be corrected to ./material.mdl.",
                        at="Attribute (info:mdl:sourceAsset) Prim </Looks/MatX/Shader>"),
            ],
        )

    # References
    async def test_reference_relative_known_path(self):
        self.assertRuleFailures(
            url=getUrl("materialLayerKnownReference.usda"),
            rule="OmniMaterialPathChecker",
            expectedFailures=[],
        )

    async def test_reference_relative_unknown_path(self):
        self.assertRuleFailures(
            url=getUrl("materialLayerUnknownReference.usda"),
            rule="OmniMaterialPathChecker",
            expectedFailures=[
                Failure("The relative path ./unknown.mdl does not exist.",
                        at="Attribute (info:mdl:sourceAsset) Prim </World/Looks/MatX/Shader>")
            ],
        )

    async def test_reference_relative_missing_dot_relative_known_path(self):
        self.assertRuleFailures(
            url=getUrl("materialLayerIncorrectReference.usda"),
            rule="OmniMaterialPathChecker",
            expectedFailures=[
                Failure("Relative path material.mdl should be corrected to ./material.mdl.",
                        at="Attribute (info:mdl:sourceAsset) Prim </World/Looks/MatX/Shader>")
            ],
        )

    @unittest.expectedFailure
    async def test_builtin_path(self):
        self.assertRuleFailures(
            url=getUrl("Materials/builtInMaterial.usda"),
            rule="OmniMaterialPathChecker",
            expectedFailures=[],
        )

    async def test_absolute_known_path(self):
        mdl_absolute_path = os.path.abspath(getUrl("Materials/material.mdl"))
        file = pathlib.Path(getUrl("Materials/absoluteMaterial.usda"))
        content = file.read_text()
        file.write_text(content.replace("replace.mdl", mdl_absolute_path))
        try:
            self.assertRuleFailures(
                url=getUrl("Materials/absoluteMaterial.usda"),
                rule="OmniMaterialPathChecker",
                expectedFailures=[],
            )
        finally:
            file.write_text(content)

    async def test_fix_path(self):
        self.assertSuggestion(
            url=getUrl("Materials/incorrectMaterial.usda"),
            rule="OmniMaterialPathChecker",
            predicate=IssuePredicates.ContainsMessage("should be corrected to")
        )

    async def test_fix_path_reference(self):
        self.assertSuggestion(
            url=getUrl("materialLayerIncorrectReference.usda"),
            rule="OmniMaterialPathChecker",
            predicate=IssuePredicates.ContainsMessage("should be corrected to")
        )
