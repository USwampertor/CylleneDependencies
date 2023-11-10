import typing
from typing import Set, Generator
from . import registerRule, BaseRuleChecker, Suggestion
from pxr import Usd, UsdGeom, Sdf


@registerRule("Omni:Layout")
class OmniDefaultPrimChecker(BaseRuleChecker):
    """
    Omniverse requires a single, active, Xformable root prim, also set to the layer's defaultPrim.
    """
    ALLOWED_PRIM_PATHS: Set[Sdf.Path] = {
        Sdf.Path("/OmniverseKit_Persp"),
        Sdf.Path("/OmniverseKit_Front"),
        Sdf.Path("/OmniverseKit_Top"),
        Sdf.Path("/OmniverseKit_Right"),
        Sdf.Path("/Render"),
    }

    @classmethod
    def activate_prim(cls, _: Usd.Stage, prim: Usd.Prim) -> None:
        prim.SetActive(True)

    @classmethod
    def deactivate_prim(cls, _: Usd.Stage, prim: Usd.Prim) -> None:
        prim.SetActive(False)

    @classmethod
    def get_all_default_prim_candidates(cls, stage: Usd.Stage) -> Generator[Usd.Prim, None, None]:
        """
        Returns:
            All prims that can be default prims.
        """
        for prim in stage.GetPseudoRoot().GetChildren():
            # Must be Xform
            if not prim.IsA(UsdGeom.Xformable):
                continue
            # Must be active
            if not prim.IsActive():
                continue
            # Cannot be abstract
            if prim.IsAbstract():
                continue
            # Cannot be prototyped
            if cls.__IsPrototype(prim):
                continue
            # Cannot be allowed
            if prim.GetPath() in cls.ALLOWED_PRIM_PATHS:
                continue
            yield prim

    @classmethod
    def get_default_prim_candidate(cls, stage: Usd.Stage) -> Usd.Prim:
        """Returns the most likely default prim or raise exception."""
        candidates = list(cls.get_all_default_prim_candidates(stage))
        if not candidates:
            raise ValueError("Can not set default prim (no prims under pseudo root).")
        if len(candidates) > 1:
            raise ValueError("Can not set default prim (potentially many candidates).")
        return candidates[0]

    @classmethod
    def update_default_prim(cls, stage: Usd.Stage, _) -> None:
        candidate = cls.get_default_prim_candidate(stage)
        stage.SetDefaultPrim(candidate)

    @classmethod
    def get_default_prim_suggestion(cls, stage: Usd.Stage) -> typing.Optional[Suggestion]:
        try:
            cls.get_default_prim_candidate(stage)
        except ValueError:
            return None
        else:
            return Suggestion(cls.update_default_prim, "Updates the default prim")

    def CheckStage(self, usdStage):
        defaultPrim = usdStage.GetDefaultPrim()
        if not defaultPrim:
            self._AddFailedCheck(
                "Stage has missing or invalid defaultPrim.",
                at=usdStage,
                suggestion=self.get_default_prim_suggestion(usdStage),
            )
            return

        if not defaultPrim.GetParent().IsPseudoRoot():
            self._AddFailedCheck(
                f'The default prim must be a root prim.',
                at=defaultPrim,
            )

        if not defaultPrim.IsA(UsdGeom.Xformable):
            self._AddFailedCheck(
                f'Invalid default prim of type "{defaultPrim.GetTypeName()}", which is not Xformable.',
                at=defaultPrim,
            )

        if not defaultPrim.IsActive():
            self._AddFailedCheck(
                f'The default prim must be active.',
                at=defaultPrim,
                suggestion=Suggestion(self.activate_prim, f"Activates Prim {defaultPrim.GetPath()}."),
            )

        if defaultPrim.IsAbstract():
            self._AddFailedCheck(
                f'The default prim cannot be abstract.',
                at=defaultPrim,
            )

        for prim in self.get_all_default_prim_candidates(usdStage):
            if prim != defaultPrim:
                self._AddFailedCheck(
                    f'Invalid root prim is a sibling of the default prim <{defaultPrim.GetName()}>. '
                    f'{self.GetDescription()}',
                    at=prim,
                    suggestion=Suggestion(self.deactivate_prim, f"Deactivates Prim {prim.GetPath()}."),
                )

    @staticmethod
    def __IsPrototype(prim):
        if hasattr(prim, 'IsPrototype'):
            return prim.IsPrototype()
        return prim.IsMaster()


@registerRule("Omni:Layout")
class OmniOrphanedPrimChecker(BaseRuleChecker):
    """
    Prims usually need a ``def`` or ``class`` specifier, not just ``over`` specifiers. However, such overs may be
    used to hold relationship targets, attribute connections, or speculative opinions.
    """

    def CheckStage(self, usdStage):
        excluded = self._GetValidOrphanOvers(usdStage)

        for prim in usdStage.TraverseAll():
            if not prim.HasDefiningSpecifier() and not prim in excluded:
                self._AddFailedCheck(
                    'Prim has an orphaned over and does not contain the target prim/property of a relationship or '
                    'connection attribute. Ignore this message if the over was meant to specify speculative opinions.',
                    at=prim,
                )

    def _GetValidOrphanOvers(self, usdStage):
        # Get all relationships and attribute connections
        targetedPrims = set()
        for prim in usdStage.TraverseAll():
            for rel in prim.GetRelationships():
                for target in rel.GetTargets():
                    targetedPrims.add(usdStage.GetPrimAtPath(target.GetPrimPath()))

            for attribute in prim.GetAttributes():
                for connection in attribute.GetConnections():
                    targetedPrims.add(usdStage.GetPrimAtPath(connection.GetPrimPath()))

        # Check ancestors for valid orphan overs
        excluded = set()
        for prim in targetedPrims:
            while prim:
                if not prim.HasDefiningSpecifier():
                    excluded.add(prim)
                prim = prim.GetParent()
        return excluded
