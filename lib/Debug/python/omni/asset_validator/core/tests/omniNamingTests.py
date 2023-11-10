import unittest

from .engineTests import getUrl
from . import ValidationRuleTestCase

class OmniNamingRulesTest(ValidationRuleTestCase):

    maxDiff = None

    @unittest.skip("OmniInvalidCharacterChecker only checks for characters that could never be serialized to USD")
    async def testOmniInvalidCharacterChecker(self):
        self.assertRuleFailures(
            url=getUrl("omniNamingFailures.usda"),
            rule="OmniInvalidCharacterChecker",
            expectedFailures=[
                ".*Prim names in Omniverse cannot include.*",
            ],
        )
