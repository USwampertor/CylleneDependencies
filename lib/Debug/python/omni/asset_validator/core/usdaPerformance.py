import enum
import typing

from . import registerRule, BaseRuleChecker

from pxr import Sdf


def _TraverseDescendents(primSpec: Sdf.PrimSpec):
    for childPrimSpec in primSpec.nameChildren:
        yield childPrimSpec
        for descendent in _TraverseDescendents(childPrimSpec):
            yield descendent


class _UsdaLayerType(enum.Enum):
    Explicit = 0 # .usda layers
    Underlying = 1 # .usd layers whose contents are ASCII


@registerRule("Usd:Performance")
class UsdAsciiPerformanceChecker(BaseRuleChecker):
    """For performance reasons, large arrays and time samples are better stored in
    crate files. This alerts users to any layers which contain large arrays or time sample
    dictionaries stored in .usda or ASCII backed .usd files."""

    # It's not uncommon for single unchanging values to be authored to the time sample
    # dictionary. Only flag time samples exceeding a relatively small threshold as
    # potential performance issues.
    _TIME_SAMPLE_LIMIT = 4
    # It's not uncommon for values which may be constant or varying over the topology
    # of a surface (primvars) to be authored as array types. Only flag array values
    # greater than a relatively small threshold as potential performance issues.
    _ARRAY_LENGTH_LIMIT = 64

    def __init__(
        self,
        verbose: bool,
        consumerLevelChecks: bool,
        assetLevelChecks: bool
    ):
        super().__init__(verbose, consumerLevelChecks, assetLevelChecks)

    def _GetUsdaLayerType(self, layer: Sdf.Layer) -> typing.Optional[_UsdaLayerType]:
        """Returns if the layer contents was authored as USDA either explicitly or indirectly."""
        fileFormat = layer.GetFileFormat()
        if fileFormat == Sdf.FileFormat.FindById("usda"):
            return _UsdaLayerType.Explicit
        if fileFormat in (Sdf.FileFormat.FindById("usd"), Sdf.FileFormat.FindById("omni")):
            if Sdf.FileFormat.FindById("usda").CanRead(layer.identifier):
                return _UsdaLayerType.Underlying
        return None

    def _ValueExceedsArrayLengthLimit(self, value) -> bool:
        if value == Sdf.ValueBlock():
            return False
        return len(value) > self._ARRAY_LENGTH_LIMIT

    def CheckLayer(self, layer: Sdf.Layer):
        """Verify the contents of the the ASCII layer won't create performance issues."""

        usdaLayerType = self._GetUsdaLayerType(layer)
        if not usdaLayerType:
            return

        largeTimeSampleSpecs = []
        longArrayValueSpecs = []

        for primSpec in _TraverseDescendents(layer.GetPrimAtPath(Sdf.Path.absoluteRootPath)):
            for attributeSpec in primSpec.attributes:
                # Prefer storing attributes with large time sample dictionaries in USDC
                if layer.GetNumTimeSamplesForPath(attributeSpec.path) > self._TIME_SAMPLE_LIMIT:
                    largeTimeSampleSpecs.append(attributeSpec)
                # Prefer storing long array valued attributes in USDC
                elif attributeSpec.typeName.isArray:
                    if (attributeSpec.HasDefaultValue() and
                        self._ValueExceedsArrayLengthLimit(attributeSpec.default)):
                        longArrayValueSpecs.append(attributeSpec)
                    else:
                        if any(
                            self._ValueExceedsArrayLengthLimit(
                                layer.QueryTimeSample(attributeSpec.path, time)
                            )
                            for time in layer.ListTimeSamplesForPath(attributeSpec.path)
                        ):
                            longArrayValueSpecs.append(attributeSpec)
        if largeTimeSampleSpecs:
            self._AddFailedCheck(
                f"{len(largeTimeSampleSpecs)} attribute(s) in '{layer.identifier}' have large numbers "
                "of time samples and should be stored in crate files for improved performance. usdcat can be used to "
                "convert .usd files containing ASCII to crate in place or .usda files to crate stored in .usdc or "
                ".usd.",
            )
        if longArrayValueSpecs:
            self._AddFailedCheck(
                f"{len(longArrayValueSpecs)} attribute(s) in '{layer.identifier}' have large array "
                "lengths and should be stored in crate files for improved performance. usdcat can be used to convert "
                ".usd files containing ASCII to crate in place or .usda files to crate stored in .usdc or .usd.",
            )
