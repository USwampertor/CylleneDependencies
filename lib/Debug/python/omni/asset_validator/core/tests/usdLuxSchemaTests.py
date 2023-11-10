import os.path
import pathlib
import unittest

from .engineTests import getUrl
from . import ValidationRuleTestCase, Failure
from omni.asset_validator.core import IssuePredicates


class UsdLuxSchemaCheckerTest(ValidationRuleTestCase):
    async def test_unprefixed_attributes(self):
        self.assertRuleFailures(
            url=getUrl("usdLuxSchema.usda"),
            rule="UsdLuxSchemaChecker",
            expectedFailures=[
                Failure("UsdLux attribute color has been renamed in USD 21.02 and should be prefixed with 'inputs:'.", at="Attribute (color) Prim </World/CylinderLight>"),
                Failure("UsdLux attribute diffuse has been renamed in USD 21.02 and should be prefixed with 'inputs:'.", at="Attribute (diffuse) Prim </World/CylinderLight>"),
                Failure("UsdLux attribute enableColorTemperature has been renamed in USD 21.02+ and should be prefixed with 'inputs:'.", at="Attribute (enableColorTemperature) Prim </World/CylinderLight>"),
                #Failure("UsdLux attribute angle has been renamed in USD 21.02 and should be prefixed with 'inputs:'.", at="Attribute (angle) Prim </World/CylinderLight>"),
                Failure("UsdLux attribute exposure has been renamed in USD 21.02 and should be prefixed with 'inputs:'.", at="Attribute (exposure) Prim </World/CylinderLight>"),
                Failure("UsdLux attribute intensity has been renamed in USD 21.02 and should be prefixed with 'inputs:'.", at="Attribute (intensity) Prim </World/CylinderLight>"),
                Failure("UsdLux attribute length has been renamed in USD 21.02 and should be prefixed with 'inputs:'.", at="Attribute (length) Prim </World/CylinderLight>"),
                Failure("UsdLux attribute normalize has been renamed in USD 21.02 and should be prefixed with 'inputs:'.", at="Attribute (normalize) Prim </World/CylinderLight>"),
                Failure("UsdLux attribute radius has been renamed in USD 21.02 and should be prefixed with 'inputs:'.", at="Attribute (radius) Prim </World/CylinderLight>"),
                Failure("UsdLux attribute shaping:cone:angle has been renamed in USD 21.02 and should be prefixed with 'inputs:'.", at="Attribute (shaping:cone:angle) Prim </World/CylinderLight>"),
                Failure("UsdLux attribute shaping:cone:softness has been renamed in USD 21.02 and should be prefixed with 'inputs:'.", at="Attribute (shaping:cone:softness) Prim </World/CylinderLight>"),
                Failure("UsdLux attribute shaping:focus has been renamed in USD 21.02 and should be prefixed with 'inputs:'.", at="Attribute (shaping:focus) Prim </World/CylinderLight>"),
                Failure("UsdLux attribute shaping:focusTint has been renamed in USD 21.02 and should be prefixed with 'inputs:'.", at="Attribute (shaping:focusTint) Prim </World/CylinderLight>"),
                #Failure("UsdLux attribute shaping:ies:file has been renamed in USD 21.02 and should be prefixed with 'inputs:'.", at="Attribute (shaping:ies:file) Prim </World/CylinderLight>"),
                Failure("UsdLux attribute shaping:ies:angleScale has been renamed in USD 21.02 and should be prefixed with 'inputs:'.", at="Attribute (shaping:ies:angleScale) Prim </World/CylinderLight>"),
                Failure("UsdLux attribute shaping:ies:normalize has been renamed in USD 21.02 and should be prefixed with 'inputs:'.", at="Attribute (shaping:ies:normalize) Prim </World/CylinderLight>"),
                Failure("UsdLux attribute specular has been renamed in USD 21.02 and should be prefixed with 'inputs:'.", at="Attribute (specular) Prim </World/CylinderLight>"),
            ],
        )

    async def test_autofix_suggestions(self):
        self.assertSuggestion(
            url=getUrl("usdLuxSchema.usda"),
            rule="UsdLuxSchemaChecker",
            predicate=IssuePredicates.ContainsMessage("UsdLux attribute")
        )

    async def test_usd_2108_no_errors(self):
        self.assertRuleFailures(
            url=getUrl("usdLuxSchema2108.usda"),
            rule="UsdLuxSchemaChecker",
            expectedFailures=[]
        )
