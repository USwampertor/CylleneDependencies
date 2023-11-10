import carb

import omni.kit.test

import omni.asset_validator.core


class MyRuleChecker(omni.asset_validator.core.BaseRuleChecker):
    """
    Check that all prims are meshes for xforms
    """

    def CheckPrim(self, prim) -> None:
        self._Msg("Checking prim <%s>." % prim.GetPath())
        if prim.GetTypeName() not in ("Mesh", "Xform"):
            self._AddFailedCheck(
                f"Prim <{prim.GetPath()}> has unsupported type '{prim.GetTypeName()}'."
            )


class ValidationRulesRegistryTest(omni.kit.test.AsyncTestCase):

    """
    A map of compliance check tests to the number of registered tests. Do not modify.
    """
    DEFAULT_CATEGORY_TEST = {
        "Basic": 10,
        "ARKit": 7,
    }
    """
    A map of Omni categories and the number of registered tests.
    """
    OMNI_CATEGORY_TEST = {
        "Omni:Layout": 2,
        "Omni:Material": 1,
        "Omni:NamingConventions": 1,
        "Usd:Schema": 4,
        "Usd:Performance": 1
    }
    ALL_CATEGORY_TEST = {
        **DEFAULT_CATEGORY_TEST,
        **OMNI_CATEGORY_TEST,
    }

    def setUp(self) -> None:
        super().setUp()
        self.__enabledCategories = carb.settings.get_settings().get("exts/omni.asset_validator.core/enabledCategories") or []
        self.__disabledCategories = carb.settings.get_settings().get("exts/omni.asset_validator.core/disabledCategories") or []
        self.__enabledRules = carb.settings.get_settings().get("exts/omni.asset_validator.core/enabledRules") or []
        self.__disabledRules = carb.settings.get_settings().get("exts/omni.asset_validator.core/disabledRules") or []

    def tearDown(self) -> None:
        super().tearDown()
        carb.settings.get_settings().set("exts/omni.asset_validator.core/enabledCategories", self.__enabledCategories)
        carb.settings.get_settings().set("exts/omni.asset_validator.core/disabledCategories", self.__disabledCategories)
        carb.settings.get_settings().set("exts/omni.asset_validator.core/enabledRules", self.__enabledRules)
        carb.settings.get_settings().set("exts/omni.asset_validator.core/disabledRules", self.__disabledRules)
        omni.asset_validator.core.ValidationRulesRegistry.deregisterRule(MyRuleChecker)

    def testCategories(self):
        self.assertCountEqual(
            omni.asset_validator.core.ValidationRulesRegistry.categories(),
            self.ALL_CATEGORY_TEST.keys()
        )
        self.assertCountEqual(
            omni.asset_validator.core.ValidationRulesRegistry.categories(enabledOnly=True),
            ("Basic",) + tuple(self.OMNI_CATEGORY_TEST.keys())
        )

    def testCategorySettings(self):
        self.assertEqual(
            carb.settings.get_settings().get("exts/omni.asset_validator.core/enabledCategories"),
            ["*"]
        )
        self.assertEqual(
            carb.settings.get_settings().get("exts/omni.asset_validator.core/disabledCategories"),
            ["ARKit"]
        )
        self.assertCountEqual(
            omni.asset_validator.core.ValidationRulesRegistry.categories(enabledOnly=True),
            ("Basic", ) + tuple(self.OMNI_CATEGORY_TEST.keys())
        )

        # explicitly enabling overrides disabling
        carb.settings.get_settings().set("exts/omni.asset_validator.core/enabledCategories", ["ARKit"])
        self.assertCountEqual(
            omni.asset_validator.core.ValidationRulesRegistry.categories(enabledOnly=True),
            self.ALL_CATEGORY_TEST.keys()
        )

        # wildcards can be used to disable, explicit enabling still takes precedence
        carb.settings.get_settings().set("exts/omni.asset_validator.core/disabledCategories", ["*"])
        self.assertEqual(
            omni.asset_validator.core.ValidationRulesRegistry.categories(enabledOnly=True),
            ("ARKit",)
        )

    def testRules(self):

        for category, numRules in self.ALL_CATEGORY_TEST.items():
            rules = omni.asset_validator.core.ValidationRulesRegistry.rules(category)
            self.assertEqual(len(rules),numRules)
            for rule in rules:
                self.assertTrue(issubclass(rule, omni.asset_validator.core.BaseRuleChecker))
                self.assertNotEqual(rule.GetDescription(), omni.asset_validator.core.BaseRuleChecker.GetDescription())
                self.assertEqual(rule, omni.asset_validator.core.ValidationRulesRegistry.rule(rule.__name__))
                self.assertEqual(category, omni.asset_validator.core.ValidationRulesRegistry.category(rule))

    def testRuleSettings(self):
        self.assertEqual(carb.settings.get_settings().get("exts/omni.asset_validator.core/enabledRules"), ["*"])
        self.assertEqual(carb.settings.get_settings().get("exts/omni.asset_validator.core/disabledRules"), [""])
        for category, numRules in (
            ("Basic", 10),
            ("ARKit", 0), # note 7 are registered
            *self.OMNI_CATEGORY_TEST.items()
        ):
            self.assertEqual(
                len(omni.asset_validator.core.ValidationRulesRegistry.rules(category,enabledOnly=True)),
                numRules
            )

        # wildcards can be used to disable rules
        carb.settings.get_settings().set("exts/omni.asset_validator.core/disabledRules", ["*Texture*",'*Kind*'])
        for category, numRules in (
            ("Basic", 7), # note 8 are registered
            ("ARKit", 0), # note 7 are registered
            *self.OMNI_CATEGORY_TEST.items()
        ):
            self.assertEqual(
                len(omni.asset_validator.core.ValidationRulesRegistry.rules(category,enabledOnly=True)),
                numRules
            )

        # explicitly enabling rules overrides disabling
        carb.settings.get_settings().set(
            "exts/omni.asset_validator.core/enabledRules",
            ["NormalMapTextureChecker","KindChecker"]
        )
        for category, numRules in (
            ("Basic", 9), # note 8 are registered
            ("ARKit", 0), # note 7 are registered
            *self.OMNI_CATEGORY_TEST.items()
        ):
            self.assertEqual(
                len(omni.asset_validator.core.ValidationRulesRegistry.rules(category,enabledOnly=True)),
                numRules
            )

        # explicitly enabling rules in a disabled category has no effect
        carb.settings.get_settings().set("exts/omni.asset_validator.core/enabledRules", ["ARKitPrimTypeChecker"])
        self.assertEqual(
            len(omni.asset_validator.core.ValidationRulesRegistry.rules("ARKit",enabledOnly=True)),
            0
        )

    def testCustomRuleRegistration(self):
        omni.asset_validator.core.ValidationRulesRegistry.registerRule(MyRuleChecker, "MyOwnRules")
        self.assertTrue("MyOwnRules" in omni.asset_validator.core.ValidationRulesRegistry.categories())
        # category is created
        self.assertTrue(MyRuleChecker in omni.asset_validator.core.ValidationRulesRegistry.rules("MyOwnRules"))
        # does not affect other categories
        self.assertFalse(MyRuleChecker in omni.asset_validator.core.ValidationRulesRegistry.rules("Basic"))
        self.assertFalse(MyRuleChecker in omni.asset_validator.core.ValidationRulesRegistry.rules("ARKit"))
        self.assertFalse(MyRuleChecker in omni.asset_validator.core.ValidationRulesRegistry.rules("Omni:NamingConventions"))
        self.assertFalse(MyRuleChecker in omni.asset_validator.core.ValidationRulesRegistry.rules("Omni:Layout"))
        self.assertFalse(MyRuleChecker in omni.asset_validator.core.ValidationRulesRegistry.rules("Usd:Schema"))

        omni.asset_validator.core.ValidationRulesRegistry.deregisterRule(MyRuleChecker)
        # category is cleaned up if all rules are deregistered
        self.assertFalse("MyOwnRules" in omni.asset_validator.core.ValidationRulesRegistry.categories())
        for category in omni.asset_validator.core.ValidationRulesRegistry.categories():
            self.assertFalse(MyRuleChecker in omni.asset_validator.core.ValidationRulesRegistry.rules(category))

        # can add to existing categories
        omni.asset_validator.core.ValidationRulesRegistry.registerRule(MyRuleChecker, "Basic")
        self.assertFalse("MyOwnRules" in omni.asset_validator.core.ValidationRulesRegistry.categories())
        self.assertTrue(MyRuleChecker in omni.asset_validator.core.ValidationRulesRegistry.rules("Basic"))

        # re-registering also re-categorizes
        omni.asset_validator.core.ValidationRulesRegistry.registerRule(MyRuleChecker, "ARKit")
        self.assertFalse("MyOwnRules" in omni.asset_validator.core.ValidationRulesRegistry.categories())
        self.assertFalse(MyRuleChecker in omni.asset_validator.core.ValidationRulesRegistry.rules("Basic"))
        self.assertTrue(MyRuleChecker in omni.asset_validator.core.ValidationRulesRegistry.rules("ARKit"))
