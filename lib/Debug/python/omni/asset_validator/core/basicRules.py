from pxr import Usd, UsdGeom, Kind, Sdf, Vt

from typing import Dict, List, Type, Callable, Generator, Tuple, TypeVar
from . import registerRule, BaseRuleChecker, Suggestion


T = TypeVar("T")

# Rules in ths file are USD requirements that are not Omniverse specific,
# and we likely want to contribute it back to Pixar at some point.


@registerRule("Basic")
class KindChecker(BaseRuleChecker):
    """
    All kinds must be registered and conform to the rules specified in the `USD Glossary`_.

    .. _`USD Glossary`: https://graphics.pixar.com/usd/release/glossary.html?#usdglossary-kind
    """
    __ROOT_KINDS = tuple(
        kind for kind in Kind.Registry.GetAllKinds()
        if Kind.Registry.IsA(kind, Kind.Tokens.assembly) or Kind.Registry.IsA(kind, Kind.Tokens.component))
    __GROUP_KINDS = tuple(kind for kind in Kind.Registry.GetAllKinds() if Kind.Registry.IsA(kind, Kind.Tokens.group))
    __VALID_KINDS = tuple(kind for kind in Kind.Registry.GetAllKinds() if kind != Kind.Tokens.model)

    @classmethod
    def is_root_model(cls, prim) -> bool:
        """Whether this is the first model in the hierarchy."""
        model: Usd.ModelAPI = Usd.ModelAPI(prim)
        if not model.IsModel():
            return False
        parent_prim: Usd.Prim = prim.GetParent()
        if parent_prim.IsPseudoRoot():
            return True
        parent_model = Usd.ModelAPI(parent_prim)
        if parent_model.IsModel():
            return False
        return True

    def CheckPrim(self, prim):
        model = Usd.ModelAPI(prim)
        kind = model.GetKind()
        if not kind:
            return
        elif self.is_root_model(prim):
            if not Kind.Registry.IsA(kind, Kind.Tokens.assembly) and not Kind.Registry.IsA(kind, Kind.Tokens.component):
                self._AddFailedCheck(
                    f'Invalid Kind "{kind}". Kind "{kind}" cannot be at the root of the Model Hierarchy. The root '
                    f'prim of a model must one of {KindChecker.__ROOT_KINDS}.',
                    at=prim,
                )
        elif kind == Kind.Tokens.model or kind not in Kind.Registry.GetAllKinds():
            self._AddFailedCheck(
                f'Invalid Kind "{kind}". Must be one of {KindChecker.__VALID_KINDS}.',
                at=prim,
            )
        elif Kind.Registry.IsA(kind, Kind.Tokens.model):
            parent = prim.GetParent()
            parent_model = Usd.ModelAPI(parent)
            parent_kind = parent_model.GetKind()
            if not Kind.Registry.IsA(parent_kind, Kind.Tokens.group):
                self._AddFailedCheck(
                    f'Invalid Kind "{kind}". Model prims can only be parented under "{KindChecker.__GROUP_KINDS}" prims,'
                    f' but parent Prim <{parent.GetName()}> has Kind "{parent_kind}".',
                    at=prim,
                )


@registerRule("Basic")
class ExtentsChecker(BaseRuleChecker):
    """
    Boundable prims have the extent attribute. For point based prims, the value of the extent must be correct at each
    time sample of the point attribute
    """

    SCHEMA_TYPE_GETTERS: List[Tuple[Type[T], Callable[[T], Usd.Attribute]]] = [
        (UsdGeom.PointBased, [lambda obj: obj.GetPointsAttr()]),
        (UsdGeom.Curves, [lambda obj: obj.GetWidthsAttr()]),
        (UsdGeom.Points, [lambda obj: obj.GetWidthsAttr()]),
    ]
    """
    Unfortunately, there's no universal way of knowing which attributes extent calculation depends on.
    Hence we hardcode the attributes per schema in SCHEMA_TYPE_GETTERS
    """
    MAX_TIME_SAMPLES: int = 5
    """
    int: The maximum number of samples to report.
    """

    @classmethod
    def get_time_samples(cls, attribute: Usd.Attribute) -> Generator[Usd.TimeCode, None, None]:
        """
        Args:
            attribute: The attribute to extract the time samples.
        Returns:
            All time samples in an attribute.
        """
        if attribute.IsValid():
            if not attribute.ValueMightBeTimeVarying():
                yield Usd.TimeCode.Default()
            else:
                for time in attribute.GetTimeSamples():
                    yield Usd.TimeCode(time)

    @classmethod
    def layers_to_apply(cls, prim: Usd.Prim) -> List[Sdf.Layer]:
        """
        Iterate Schemas and their respective attributes and fix those layers first.
        """
        layers: List[Sdf.Layer] = []
        for SchemaType, Getters in cls.SCHEMA_TYPE_GETTERS:
            instance: SchemaType = SchemaType(prim)
            if not instance:
                continue
            for getter in Getters:
                attribute: Usd.Attribute = getter(instance)
                for spec in attribute.GetPropertyStack(time=Usd.TimeCode.Default()):
                    layers.append(spec.layer)
        return layers

    @classmethod
    def compute_extents(cls, _: Usd.Stage, prim: Usd.Prim) -> None:
        """
        Compute the extents of a specific prim.
        """
        boundable: UsdGeom.Boundable = UsdGeom.Boundable(prim)

        time_code_extent: Dict[Usd.TimeCode, Vt.Vec3fArray] = {}
        for SchemaType, Getters in cls.SCHEMA_TYPE_GETTERS:
            instance: SchemaType = SchemaType(prim)
            if not instance:
                continue
            for getter in Getters:
                attribute: Usd.Attribute = getter(instance)
                for time in cls.get_time_samples(attribute):
                    time_code_extent[time] = boundable.ComputeExtentFromPlugins(boundable, time)

        attribute: Usd.Attribute = boundable.CreateExtentAttr()
        for time, extent in time_code_extent.items():
            attribute.Set(extent, time)

    def CheckPrim(self, prim: Usd.Prim) -> None:
        boundable: UsdGeom.Boundable = UsdGeom.Boundable(prim)
        if not boundable:
            return
        attribute: Usd.Attribute = boundable.GetExtentAttr()
        # Has no value
        if not attribute.HasValue():
            self._AddFailedCheck(
                "Prim does not have any extent value",
                at=prim,
                suggestion=Suggestion(
                    message="Compute the extents",
                    callable=self.compute_extents,
                    at=self.layers_to_apply(prim),
                )
            )
            return  # Nothing more to do
        # Retrieve values
        authored_vs_computed_cache: Dict[Usd.TimeCode, bool] = {}
        for time in self.get_time_samples(attribute):
            authored_extent: Vt.Vec3fArray = boundable.GetExtentAttr().Get(time)
            computed_extent: Vt.Vec3fArray = boundable.ComputeExtentFromPlugins(boundable, time)
            authored_vs_computed_cache[time] = computed_extent == authored_extent
        # Check extent at each time sample
        authored_vs_computed: Dict[Usd.TimeCode, bool] = {}
        for SchemaType, Getters in self.SCHEMA_TYPE_GETTERS:
            instance: SchemaType = SchemaType(prim)
            if not instance:
                continue
            for getter in Getters:
                attribute: Usd.Attribute = getter(instance)
                for time in self.get_time_samples(attribute):
                    try:
                        authored_vs_computed[time] = authored_vs_computed_cache[time]
                    except KeyError:
                        authored_extent: Vt.Vec3fArray = boundable.GetExtentAttr().Get(time)
                        computed_extent: Vt.Vec3fArray = boundable.ComputeExtentFromPlugins(boundable, time)
                        authored_vs_computed[time] = computed_extent == authored_extent
        # Get all timestamps where values differ
        time_codes: List[Usd.TimeCode] = []
        for time, flag in authored_vs_computed.items():
            if not flag:
                time_codes.append(time)

        if Usd.TimeCode.Default() in time_codes:
            self._AddFailedCheck(
                f"Prim has incorrect extent value",
                at=prim,
                suggestion=Suggestion(
                    message="Recompute the extents",
                    callable=self.compute_extents,
                    at=self.layers_to_apply(prim),
                )
            )
        elif len(time_codes) == 1:
            self._AddFailedCheck(
                f"Incorrect extent value for prim at time {time_codes[0].GetValue()}",
                at=prim,
                suggestion=Suggestion(
                    message="Recompute the extents",
                    callable=self.compute_extents,
                    at=self.layers_to_apply(prim),
                )
            )
        elif len(time_codes) > 1:
            values = ", ".join(map(lambda time_code: str(time_code.GetValue()), time_codes[:self.MAX_TIME_SAMPLES]))
            if len(time_codes) > self.MAX_TIME_SAMPLES:
                values = f"{values}, ..."
            self._AddFailedCheck(
                f"Incorrect extent value for prim at multiple times (i.e. {values})",
                at=prim,
                suggestion=Suggestion(
                    message="Recompute the extents",
                    callable=self.compute_extents,
                    at=self.layers_to_apply(prim),
                )
            )


@registerRule("Basic")
class TypeChecker(BaseRuleChecker):
    """
    All prims must have a type defined.
    """

    def CheckPrim(self, prim):
        if prim.IsDefined() and not prim.GetTypeName():
            self._AddFailedCheck(
                f'Missing type',
                at=prim,
            )
