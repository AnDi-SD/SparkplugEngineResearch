"""Pure queue/scope audit tests; no files, executable or production DB writes."""
import unittest
from audit_native_scope_dependencies import audit_dependencies
class ScopeDependencyTests(unittest.TestCase):
    def fixture(self,names=('spBase',),platform='pc'):
        return {'items':[{'id':'first','platform':platform,'classes':list(names)}]}, {'scopeId':'v1','groups':[{'classes':['spBase']}]}, {'spBase':{'pc','ps2'},'spNew':{'pc'}}
    def test_known_members_pass_without_behavior_credit(self):
        result=audit_dependencies(*self.fixture());self.assertTrue(result['passed']);self.assertNotIn('score',result)
    def test_unrated_dependency_still_requires_scope(self):
        result=audit_dependencies(*self.fixture(('spBase','spNew')));self.assertFalse(result['passed']);self.assertEqual(result['missingScopeMembers'][0]['className'],'spNew')
    def test_repeated_dependency_is_deduplicated(self):
        config,scope,catalog=self.fixture(('spNew',));config['items'].append(dict(config['items'][0],id='second'))
        result=audit_dependencies(config,scope,catalog);self.assertEqual(result['checkedClassPlatformPairs'],1);self.assertEqual(result['missingScopeMembers'][0]['workItems'],['first','second'])
    def test_platform_presence_not_inferred(self):
        result=audit_dependencies(*self.fixture(('spNew',),'ps2'));self.assertEqual(result['invalidCatalogReferences'][0]['reason'],'absent_on_platform')
    def test_unknown_original_name_is_not_invented(self):
        result=audit_dependencies(*self.fixture(('madeUp',)));self.assertEqual(result['invalidCatalogReferences'][0]['reason'],'not_registered')
if __name__=='__main__':unittest.main()
