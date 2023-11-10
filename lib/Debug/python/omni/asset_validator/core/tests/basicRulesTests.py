from .engineTests import getUrl
from . import ValidationRuleTestCase, Failure
from omni.asset_validator.core import IssuePredicates


class BasicRulesTest(ValidationRuleTestCase):

    maxDiff = None

    async def test_extents(self):
        # Given
        prim_cube_no_extent = "Prim </World/ParentTransform/CubeNoExtent>"
        prim_cube_bad_extent = "Prim </World/CubeIncorrectExtent>"
        prim_mesh_no_extent = "Prim </World/deforming_mesh_no_extent>"
        prim_mesh_incorrect = "Prim </World/deforming_mesh_incorrect_extent_samples>"
        prim_curve_0 = "Prim </World/curve_0>"
        prim_points_0 = "Prim </World/points_0>"
        # When / Then
        self.assertRuleFailures(
            url=getUrl("extent.usda"),
            rule="ExtentsChecker",
            expectedFailures=[
                Failure('Prim does not have any extent value.*', at=prim_cube_no_extent),
                Failure('Prim has incorrect extent value.*', at=prim_cube_bad_extent),
                Failure('Prim does not have any extent value.*', at=prim_mesh_no_extent),
                Failure(r'Incorrect extent value for prim at multiple times \(i.e. 1.0, 2.0\).*', at=prim_mesh_incorrect),
                Failure('Prim does not have any extent value.*', at=prim_curve_0),
                Failure(r'Incorrect extent value for prim at time 2.0.*', at=prim_points_0),
            ],
        )
        self.assertRuleFailures(
            url=getUrl("curves.usda"),
            rule="ExtentsChecker",
            expectedFailures=[],
        )

    async def test_extents_autofix(self):
        self.assertSuggestion(
            url=getUrl("extent.usda"),
            rule="ExtentsChecker",
            predicate=None
        )

    async def testMissingReferenceChecker(self):
        self.assertRuleFailures(
            url=getUrl("basicFailures.usda"),
            rule="MissingReferenceChecker",
            expectedFailures=[
                Failure(".*Could not open asset.*"),
                Failure(".*unresolvable external dependency.*"),
            ],
        )

    async def testStageMetadataChecker(self):
        self.assertRuleFailures(
            url=getUrl("basicFailures.usda"),
            rule="StageMetadataChecker",
            expectedFailures=[
                Failure(".*upAxis.*"),
                Failure(".*metersPerUnit.*"),
                Failure(".*invalid defaultPrim.*"),
            ],
        )

    async def testPrimEncapsulationChecker(self):
        self.assertRuleFailures(
            url=getUrl("basicFailures.usda"),
            rule="PrimEncapsulationChecker",
            expectedFailures=[
                Failure(".*has an ancestor prim that is also a Gprim.*"),
            ],
        )

    async def testTextureChecker(self):
        self.assertRuleFailures(
            url=getUrl("basicFailures.usda"),
            rule="TextureChecker",
            expectedFailures=[
                Failure(".*has unknown file format.*"),
            ],
        )

    async def testKindChecker(self):
        # Given
        prim_root = "Prim </Root>"
        prim_nested_sphere = "Prim </Root/Sphere/NestedSphere>"
        prim_bad_sphere = "Prim </Root/BadSphere>"
        prim_leaf_sphere = "Prim </Root/MissingKind/LeafSphere>"
        prim_not_model = "Prim </NotAModel/StillNotAModel>"
        prim_base_kind = "Prim </NotAModel/StillNotAModel/BaseKind>"
        prim_also_not_model = "Prim </NotAModel/Subcomponent/AlsoNotAModel>"
        prim_bad_component = "Prim </NotAModel/Subcomponent/BadComponent>"
        # When / Then
        self.assertRuleFailures(
            url=getUrl("basicFailures.usda"),
            rule="KindChecker",
            expectedFailures=[
                Failure('Invalid Kind "group".*Kind "group" cannot be at the root of the Model Hierarchy.*', at=prim_root),
                Failure('Invalid Kind "component".*Model prims can only be parented under.*', at=prim_nested_sphere),
                Failure('Invalid Kind "fake".*Must be one of', at=prim_bad_sphere),
                Failure('Invalid Kind "component".*Model prims can only be parented under.*', at=prim_leaf_sphere),
                Failure('Invalid Kind "assembly".*Model prims can only be parented under.*', at=prim_not_model),
                Failure('Invalid Kind "model".*Must be one of.*', at=prim_base_kind),
                Failure('Invalid Kind "group".*Model prims can only be parented under.*', at=prim_also_not_model),
                Failure('Invalid Kind "component".*Model prims can only be parented under.*', at=prim_bad_component),
            ],
        )
        self.assertRuleFailures(
            url=getUrl("kindChecker.usda"),
            rule="KindChecker",
            expectedFailures=[
            ],
        )

    async def testMissingTypeChecker(self):
        self.assertRuleFailures(
            url=getUrl("untyped.usda"),
            rule="TypeChecker",
            expectedFailures=[
                Failure(".*Missing type.*"),
            ],
        )
