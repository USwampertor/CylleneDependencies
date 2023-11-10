import omni.kit.test

from omni.asset_validator.core import BaseRuleChecker


class BaseRuleCheckerTestCase(omni.kit.test.AsyncTestCase):

    def test_GetDescription(self):
        class Test0(BaseRuleChecker):
            pass

        class Test1(BaseRuleChecker):
            """"""

        class Test2(BaseRuleChecker):
            """ Single line doc. """

        class Test3(BaseRuleChecker):
            """ First line doc.
            Args
                a: First argument
            """

        class Test4(BaseRuleChecker):
            """
            Second line doc.
            Args
                a: First argument
            """

        class Test5(BaseRuleChecker):
            @classmethod
            def GetDescription(cls):
                return "\n".join(["Third line doc.", "Args", "    a: First argument"])

        self.assertEqual(Test0.GetDescription(), "Base description. This should be re-implemented by derived classes")
        self.assertEqual(Test1.GetDescription(), "Base description. This should be re-implemented by derived classes")
        self.assertEqual(Test2.GetDescription(), "Single line doc. ")
        self.assertEqual(Test3.GetDescription(), "First line doc.\nArgs\n    a: First argument")
        self.assertEqual(Test4.GetDescription(), "Second line doc.\nArgs\n    a: First argument")
        self.assertEqual(Test5.GetDescription(), "Third line doc.\nArgs\n    a: First argument")
