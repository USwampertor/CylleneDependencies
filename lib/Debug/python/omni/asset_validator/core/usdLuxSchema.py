from typing import Set

from . import registerRule, BaseRuleChecker, Suggestion, AuthoringLayers

from pxr import Sdf, Usd, UsdLux


@registerRule("Usd:Schema")
class UsdLuxSchemaChecker(BaseRuleChecker):
    """
    In USD 21.02, Lux attributes were prefixed with inputs: to make them connectable. This rule checker ensure that
    all UsdLux attributes have the appropriate prefix.
    """
    LUX_ATTRIBUTES: Set[str] = {
        'angle', 'color', 'temperature', 'diffuse', 'specular', 'enableColorTemperature',
        'exposure', 'height', 'width', 'intensity', 'length', 'normalize', 'radius', 'shadow:color',
        'shadow:distance', 'shadow:enable', 'shadow:falloff', 'shadow:falloffGamma', 'shaping:cone:angle',
        'shaping:cone:softness', 'shaping:focus', 'shaping:focusTint', 'shaping:ies:angleScale',
        'shaping:ies:file', 'shaping:ies:normalize', 'texture:format'
    }
    """
    List of attributes to check prefixes for.
    """

    @classmethod
    def fix_attribute_name(cls, _: Usd.Stage, attribute: Usd.Attribute) -> None:
        attribute_value = attribute.Get()
        prim: Usd.Prim = attribute.GetPrim()
        old_name = attribute.GetName()
        new_name = f"inputs:{old_name}"
        new_attribute = prim.CreateAttribute(new_name, attribute.GetTypeName())
        new_attribute.Set(attribute_value)
        if attribute.ValueMightBeTimeVarying():
            time_samples = attribute.GetTimeSamples()
            for time in time_samples:
                timecode = Usd.TimeCode(time)
                new_attribute.Set(attribute.Get(timecode), timecode)

        # prim.RemoveProperty(old_name)

    def CheckPrim(self, prim):
        if (
            not (hasattr(UsdLux, 'Light') and prim.IsA(UsdLux.Light)) and
            not (hasattr(UsdLux, 'LightAPI') and prim.HasAPI(UsdLux.LightAPI))
        ):
            return

        attributes = prim.GetAuthoredAttributes()
        attribute_names = [attr.GetName() for attr in attributes]
        for attribute in attributes:
            attribute_value = attribute.Get()

            if attribute_value is None:
                continue

            if not attribute.GetName() in self.LUX_ATTRIBUTES:
                continue

            if f"inputs:{attribute.GetName()}" in attribute_names:
                continue

            self._AddFailedCheck(
                f"UsdLux attribute {attribute.GetName()} has been renamed in USD 21.02 and should be prefixed with 'inputs:'.",
                at=attribute,
                suggestion=Suggestion(
                    callable=self.fix_attribute_name,
                    message=f"Creates a new attribute {attribute.GetName()} prefixed with inputs: for compatibility.",
                    at=AuthoringLayers(attribute),
                )
            )
