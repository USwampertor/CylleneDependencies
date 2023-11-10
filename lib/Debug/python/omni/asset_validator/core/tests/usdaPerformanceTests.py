import pathlib
import tempfile

from pxr import Sdf, Usd

from .engineTests import getUrl
from . import ValidationRuleTestCase, Failure

class UsdAsciiPerformanceCheckerTest(ValidationRuleTestCase):
    async def test_pass(self):
        """Verify layers without large numbers of time samples and array lengths pass."""
        self.assertRuleFailures(
            url=getUrl("usdaPerformancePass.usda"),
            rule="UsdAsciiPerformanceChecker",
            expectedFailures=[],
        )

    async def test_failure(self):
        """Verify layers with large numbers of time samples and array lengths are flagged."""
        self.assertRuleFailures(
            url=getUrl("usdaPerformanceFail.usda"),
            rule="UsdAsciiPerformanceChecker",
            expectedFailures=[
                Failure(r"2 attribute\(s\) in '.+' have large numbers of time samples"),
                Failure(r"1 attribute\(s\) in '.+' have large array lengths"),
            ],
        )

    async def test_underlying_ascii_failure(self):
        """Copy the layer to a .usd layer with ASCII contents and verify the rule still fails."""
        source = Sdf.Layer.FindOrOpen(getUrl("usdaPerformanceFail.usda"))
        with tempfile.TemporaryDirectory() as directory:
            asciiUnderlyingLayer = Sdf.Layer.CreateNew(
                str(pathlib.Path(directory) / "underlying_ascii.usd"),
                args = {"format" : "usda"}
            )
            asciiUnderlyingLayer.TransferContent(source)
            self.assertRuleFailures(
                asciiUnderlyingLayer.identifier,
                rule="UsdAsciiPerformanceChecker",
                expectedFailures=[
                    Failure(r"2 attribute\(s\) in '.+' have large numbers of time samples"),
                    Failure(r"1 attribute\(s\) in '.+' have large array lengths"),
                ],
            )

    async def test_underlying_crate_pass(self):
        """Copy the layer to a .usd layer with crate contents and verify the rule passes."""
        source = Sdf.Layer.FindOrOpen(getUrl("usdaPerformanceFail.usda"))
        with tempfile.TemporaryDirectory() as directory:
            crateUnderlyingLayer = Sdf.Layer.CreateNew(
                str(pathlib.Path(directory) / "underlying_crate.usd"),
                args = {"format" : "usdc"}
            )
            crateUnderlyingLayer.TransferContent(source)
            self.assertRuleFailures(
                crateUnderlyingLayer.identifier,
                rule="UsdAsciiPerformanceChecker",
                expectedFailures=[],
            )