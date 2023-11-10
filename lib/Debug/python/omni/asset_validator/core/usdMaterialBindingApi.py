from . import registerRule, BaseRuleChecker, Suggestion, AuthoringLayers

from pxr import Usd, UsdShade, Sdf


@registerRule("Usd:Schema")
class UsdMaterialBindingApi(BaseRuleChecker):
    """
    Rule ensuring that the MaterialBindingAPI is applied on all prims that have a material binding property.
    """

    @classmethod
    def apply_material_binding_api_fix(cls, _: Usd.Stage, prim) -> None:
        UsdShade.MaterialBindingAPI.Apply(prim)

    def CheckPrim(self, prim):
        if prim.HasAPI(UsdShade.MaterialBindingAPI):
            return

        has_material_binding_rel = any(rel.GetName().startswith(UsdShade.Tokens.materialBinding)
                                       for rel in prim.GetRelationships())
        if has_material_binding_rel and not prim.HasAPI(UsdShade.MaterialBindingAPI):
            self._AddFailedCheck(
                f"Prim '{prim.GetName()}' has a material binding but does not have the MaterialBindingApi.",
                at=prim,
                suggestion=Suggestion(
                    callable=self.apply_material_binding_api_fix,
                    message=f"Applies the material binding API.",
                    at=AuthoringLayers([
                        relationship
                        for relationship in prim.GetRelationships()
                        if relationship.GetName().startswith(UsdShade.Tokens.materialBinding)
                    ]),
                ),
            )
