from . import registerRule, BaseRuleChecker, Suggestion, AuthoringLayers

from pxr import Usd, UsdShade, Sdf


@registerRule("Usd:Schema")
class UsdDanglingMaterialBinding(BaseRuleChecker):
    """
    Rule ensuring that the bound material exists in the scene.
    """

    @classmethod
    def apply_dangling_material_binding_fix(cls, _: Usd.Stage, prim) -> None:
        api = UsdShade.MaterialBindingAPI(prim)
        api.UnbindAllBindings()

    def CheckPrim(self, prim):
        rel = prim.GetRelationship(UsdShade.Tokens.materialBinding)

        if rel:
            stage = prim.GetStage()
            for target in rel.GetTargets():
                if not stage.GetPrimAtPath(target):
                    self._AddFailedCheck(
                        f"Prim '{prim.GetName()}' has a material binding to '{target}' but that location does not exist",
                        at=prim,
                        suggestion=Suggestion(
                            callable=self.apply_dangling_material_binding_fix,
                            message=f"Remove material bindings from the prim.",
                            at=AuthoringLayers(rel),
                        )
                    )
                    break
