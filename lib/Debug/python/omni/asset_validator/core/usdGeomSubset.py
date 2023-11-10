from . import registerRule, BaseRuleChecker, Suggestion, AuthoringLayers

from pxr import Usd, UsdGeom, UsdShade, Sdf


@registerRule("Usd:Schema")
class UsdGeomSubsetChecker(BaseRuleChecker):
    """
    Ensures that a valid family name attribute is set for every UsdGeomSubset that has a material binding.
    """

    @classmethod
    def apply_family_name_fix(cls, _: Usd.Stage, prim) -> None:
        subset = UsdGeom.Subset(prim)
        subset.CreateFamilyNameAttr().Set(UsdShade.Tokens.materialBind)

    def CheckPrim(self, prim):
        if not prim.IsA(UsdGeom.Subset):
            return


        hasMaterialBindingRel = False
        hasFamilyName = False

        for prop in prim.GetProperties():
            hasMaterialBindingRel = hasMaterialBindingRel or prop.GetName().startswith(UsdShade.Tokens.materialBinding)
            hasFamilyName = hasFamilyName or prop.GetName().startswith("familyName")

        subset = UsdGeom.Subset(prim)

        if (
            hasMaterialBindingRel and
            (not hasFamilyName or subset.GetFamilyNameAttr().Get() != UsdShade.Tokens.materialBind)
        ):
            self._AddFailedCheck(
                f"GeomSubset '{prim.GetName()}' has a material binding but no valid family name attribute.",
                at=prim,
                suggestion=Suggestion(
                    callable=self.apply_family_name_fix,
                    message=f"Adds the family name attribute.",
                    at=AuthoringLayers([
                        relationship
                        for relationship in prim.GetRelationships()
                        if relationship.GetName().startswith(UsdShade.Tokens.materialBinding)
                    ]),
                )
            )
