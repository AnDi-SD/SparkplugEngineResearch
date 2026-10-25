import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import ps2_vu_dataflow as flow
from ps2_vu_dataflow import analyze, branch, direct_graph, footprint, pair_priority


def word(fs=0, ft=0, fd=0, mask='xyzw'):
    bits = {'x': 24, 'y': 23, 'z': 22, 'w': 21}
    return (fs << 11) | (ft << 16) | (fd << 6) | sum(1 << bits[c] for c in mask)


def row(lower='MOVE', code=0, upper='NOP', upper_code=0):
    return dict(lowerWord=code, upperWord=upper_code,
        lowerSymbol='disExternalVUMI_' + lower, lowerText=lower,
        upperSymbol='disExternalVUMI_' + upper, upperText=upper)


class OperandFactsTests(unittest.TestCase):
    def test_broadcast_accumulator_and_selected_destination(self):
        facts = footprint('MADDy', word(2, 3, 4, 'x'), True)
        self.assertTrue(facts['classified'])
        self.assertEqual(set(facts['reads']), {'vf02.x', 'vf03.y', 'ACC.x', 'MAC', 'STATUS'})
        self.assertEqual(set(facts['writes']), {'vf04.x', 'MAC', 'STATUS'})
        acc = footprint('SUBAw', word(2, 3, 4, 'xz'), True)
        self.assertEqual(set(acc['writes']), {'ACC.x', 'ACC.z', 'MAC', 'STATUS'})

    def test_conversion_uses_ft_and_constant_writes_are_ignored(self):
        facts = footprint('FTOI4', word(2, 3, 4, 'yw'), True)
        self.assertEqual(facts['writes'], ['vf03.w', 'vf03.y'])
        facts = footprint('ADD', word(0, 1, 0, 'xy'), True)
        self.assertEqual(facts['ignoredConstantWrites'], ['vf00.x', 'vf00.y'])
        self.assertEqual(facts['constantReads'], ['vf00.x', 'vf00.y'])
        self.assertNotIn('vf00.x', facts['reads'])

    def test_outer_products_and_clip_ignore_destination_mask(self):
        facts = footprint('OPMSUB', word(1, 2, 3, ''), True)
        self.assertTrue({'vf03.x', 'vf03.y', 'vf03.z'} <= set(facts['writes']))
        clip = footprint('CLIP', word(1, 2, 3, ''), True)
        self.assertEqual(set(clip['reads']), {'vf01.x', 'vf01.y', 'vf01.z', 'vf02.w', 'CLIP'})

    def test_integer_fields_mtir_and_mfir_are_raw_transfers(self):
        facts = footprint('IADD', word(17, 18, 19), False)
        self.assertEqual(facts['reads'], ['vi01', 'vi02'])
        self.assertEqual(facts['writes'], ['vi03'])
        facts = footprint('MTIR', word(4, 5, 0, 'y'), False)
        self.assertEqual(facts['writes'], ['vi05'])
        self.assertIn('low16', facts['expression'])
        facts = footprint('MFIR', word(4, 5, 0, 'xz'), False)
        self.assertEqual(facts['reads'], ['vi04'])
        self.assertEqual(facts['writes'], ['vf05.x', 'vf05.z'])
        self.assertIn('sign_extend16', facts['expression'])

    def test_memory_postincrement_width_and_raw_unsigned_address(self):
        facts = footprint('LQI', word(3, 4, 0, 'zw'), False)
        self.assertEqual(facts['memory'][0]['base'], 'vi03')
        self.assertEqual(facts['memory'][0]['baseUpdate'], 1)
        self.assertEqual(facts['memory'][0]['baseInterpretation'], 'u16')
        self.assertIn('vi03', facts['writes'])
        self.assertEqual(facts['memory'][0]['lanes'], 'zw')
        facts = footprint('ISW', word(3, 4, 0, 'y') | 0x7FF, False)
        self.assertEqual(facts['memory'][0]['qwordOffset'], -1)
        self.assertEqual(facts['memory'][0]['elementBits'], 32)
        self.assertIn('zero upper16', facts['notes'][0])

    def test_mr32_rotation_and_q_pending_are_explicit(self):
        facts = footprint('MR32', word(2, 3, 0, 'xw'), False)
        self.assertEqual(facts['reads'], ['vf02.x', 'vf02.y'])
        facts = footprint('DIV', word(2, 3, 0, ''), False)
        self.assertIn('Q.pending', facts['writes'])
        self.assertNotIn('Q', facts['writes'])
        self.assertEqual(footprint('WAITQ', 0, False)['reads'], ['Q.pending'])

    def test_constant_memory_base_suppresses_address_update(self):
        facts = footprint('LQD', word(0, 4, 0, 'x'), False)
        self.assertEqual(facts['memory'][0]['requestedBaseUpdate'], -1)
        self.assertEqual(facts['memory'][0]['baseUpdate'], 0)
        self.assertFalse(facts['memory'][0]['updateBeforeAccess'])
        self.assertEqual(facts['ignoredConstantWrites'], ['vi00'])
        self.assertIn('base update +0 (requested -1)', facts['expression'])

    def test_immediate_does_not_execute_lower_opcode(self):
        facts = footprint('JR', 0xFFFFFFFF, False, immediate=True)
        self.assertEqual(facts['reads'], [])
        self.assertEqual(facts['writes'], ['I'])
        pair = row('JR', 0xFFFFFFFF, 'ADDi', word(2, 3, 4, 'x') | 0x80000000)
        result = analyze([pair])
        self.assertIn('I', result['instructions'][0]['upper']['reads'])
        self.assertIn('I', result['instructions'][0]['lower']['writes'])
        self.assertIsNone(branch(pair, 0))

    def test_unknown_opcode_is_not_successful_nop(self):
        facts = footprint('NOT_A_VU_OPCODE', 0, False)
        self.assertFalse(facts['classified'])
        self.assertIsNone(facts['expression'])


class PairPriorityTests(unittest.TestCase):
    def test_disjoint_masks_discard_entire_lower_including_address_update(self):
        facts = pair_priority(row('LQI', word(3, 4, 0, 'w'), 'ADD', word(1, 2, 4, 'x')))
        self.assertTrue(facts['discardedLower'])
        self.assertEqual(facts['effectiveLowerWrites'], [])
        self.assertEqual(facts['effectiveLowerMemory'], [])

    def test_empty_upper_mask_still_has_metadata_destination(self):
        facts = pair_priority(row('MOVE', word(2, 4, 0, 'w'), 'ADD', word(1, 2, 4, '')))
        self.assertTrue(facts['discardedLower'])

    def test_vf_zero_is_not_an_upper_write_hazard(self):
        facts = pair_priority(row('MOVE', word(2, 0), 'ADD', word(1, 2, 0)))
        self.assertFalse(facts['discardedLower'])

    def test_conflicting_read_uses_old_whole_vf(self):
        facts = pair_priority(row('SQI', word(4, 3, 0, 'w'), 'ADD', word(1, 2, 4, 'x')))
        self.assertFalse(facts['discardedLower'])
        self.assertEqual(facts['lowerOldVFRegisters'], ['vf04'])
        self.assertTrue(facts['effectiveLowerMemory'])

    def test_clip_write_discards_lower_and_read_backs_up_old_clip(self):
        self.assertTrue(pair_priority(row('FCSET', 0, 'CLIP'))['discardedLower'])
        self.assertTrue(pair_priority(row('FCAND', 0, 'CLIP'))['lowerOldClip'])

    def test_immediate_publishes_after_upper_and_unknown_remains_unresolved(self):
        facts = pair_priority(row('JR', 0xffffffff, 'ADDi', word(2, 3, 4) | 0x80000000))
        self.assertEqual(facts['lowerExecution'], 'publish-I-after-upper')
        self.assertEqual(facts['effectiveLowerWrites'], ['I'])
        self.assertFalse(pair_priority(row('UNKNOWN'))['resolved'])


class ControlFactsTests(unittest.TestCase):
    def test_unconditional_bal_has_link_and_no_invented_return_edge(self):
        rows = [row('BAL', 2), row(), row(), row('MOVE', upper_code=0x40000000), row()]
        control = branch(rows[0], 0)
        self.assertEqual(control['target'], 3)
        self.assertEqual(control['linkReturn'], 2)
        graph = direct_graph(rows)
        self.assertEqual(graph['reachableIndices'], [0, 1, 3, 4])
        self.assertEqual(graph['exits'][0]['index'], 4)

    def test_conditional_delay_has_separate_contexts(self):
        rows = [row('IBNE', 2), row(), row(upper_code=0x40000000), row()]
        graph = direct_graph(rows)
        self.assertTrue([1, 3, False] in graph['states'])
        self.assertTrue([1, 2, False] in graph['states'])
        self.assertTrue(any(e['source'] == [1, 3, False] and e['target'][0] == 3 for e in graph['edges']))

    def test_end_delay_and_consecutive_end_reset(self):
        rows = [row(upper_code=0x40000000), row(upper_code=0x40000000), row(), row()]
        graph = direct_graph(rows)
        self.assertEqual(graph['reachableIndices'], [0, 1, 2])
        self.assertEqual(graph['exits'][0]['index'], 2)

    def test_indirect_and_branch_in_delay_are_gaps(self):
        graph = direct_graph([row('JR', word(fs=1)), row(), row()])
        self.assertEqual(graph['reachableIndices'], [0, 1])
        self.assertIn('indirect target', graph['unresolved'][0]['reason'])
        graph = direct_graph([row('B', 1), row('B', 1), row()])
        self.assertIn('branch delay', graph['unresolved'][0]['reason'])

    def test_signed_target_and_environment_flags_are_not_guessed(self):
        control = branch(row('B', 0x7FE), 0)
        self.assertEqual(control['target'], -1)
        graph = direct_graph([row(upper_code=0x10000000), row()])
        self.assertIn('environment', graph['unresolved'][0]['reason'])
        with self.assertRaises(ValueError): direct_graph([], 0)

    def test_unknown_control_does_not_invent_fallthrough(self):
        graph = direct_graph([row('UNKNOWN'), row()])
        self.assertEqual(graph['reachableIndices'], [0])
        self.assertEqual(graph['edges'], [])
        self.assertIn('unsupported opcode', graph['unresolved'][0]['reason'])

    def test_branch_expires_before_end_stop(self):
        rows = [row('B', 2, upper_code=0x40000000), row(), row(), row(upper_code=0x40000000), row()]
        result = analyze(rows)
        exit_ = result['directControlFlow']['exits'][0]
        self.assertEqual(exit_['postPairState'], dict(nextIndex=3, pendingTarget=None, endCountdown=0))
        self.assertEqual(exit_['referenceResumeIndex'], 3)
        self.assertEqual([g['entry'] for g in result['stoppedUnitContinuations']['segments']], [0, 3])

    def test_branch_in_end_delay_keeps_pair_state_and_outer_resume_separate(self):
        rows = [row(upper_code=0x40000000), row('B', 2), row(), row(), row()]
        exit_ = direct_graph(rows)['exits'][0]
        self.assertEqual(exit_['postPairState'], dict(nextIndex=2, pendingTarget=4, endCountdown=0))
        self.assertEqual(exit_['referenceResumeIndex'], 4)

    def test_conditional_end_delay_retains_both_resume_outcomes(self):
        rows = [row(upper_code=0x40000000), row('IBNE', 2), row(), row(), row()]
        graph = direct_graph(rows)
        self.assertEqual({e['referenceResumeIndex'] for e in graph['exits']}, {3, 4})

    def test_indirect_end_delay_does_not_invent_resume(self):
        rows = [row(upper_code=0x40000000), row('JR', word(fs=1)), row()]
        result = analyze(rows)
        self.assertFalse(result['directControlFlow']['exits'][0]['resumeResolved'])
        self.assertEqual(len(result['stoppedUnitContinuations']['unresolved']), 1)

    def test_continuation_cycle_visited_once_and_segment_bound_explicit(self):
        rows = [row(upper_code=0x40000000), row('B', 0x7fe)]
        continued = analyze(rows)['stoppedUnitContinuations']
        self.assertEqual([g['entry'] for g in continued['segments']], [0])
        self.assertEqual(continued['resumeEdges'][0]['resumeEntry'], 0)
        rows = [row(upper_code=0x40000000) if i % 2 == 0 else row() for i in range(140)]
        continued = analyze(rows)['stoppedUnitContinuations']
        self.assertEqual(len(continued['segments']), 64)
        self.assertIn('segment bound', continued['unresolved'][0]['reason'])


class ArtifactBoundaryTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.isa = self.root / 'isa'
        self.isa.mkdir()
        self.output = self.root / 'local-data' / 'flow'
        self.report = self.root / '.private' / 'report.json'
        self.contract = self.root / 'contract.json'
        self.contract.write_text(json.dumps(dict(files=[])), encoding='utf-8')
        self.prior = self.root / 'prior.json'
        self.programs = []
        self.add_program('one.isa.json')
        self.addCleanup(patch.stopall)
        patch.object(flow, 'ROOT', self.root).start()
        patch.object(flow, 'CONTRACT', self.contract).start()

    def add_program(self, name):
        rows = [row(upper_code=0x40000000), row()]
        for index, item in enumerate(rows): item['pc'] = index * 8
        raw = json.dumps(dict(instructions=rows)).encode()
        (self.isa / name).write_bytes(raw)
        self.programs.append(dict(files={name: flow.digest(raw)},
            inputSha256=flow.digest(flow.reconstruct_encodings(rows))))

    def run_artifact(self):
        self.prior.write_text(json.dumps(dict(kind='bounded-static-vu-isa-decode',
            programs=self.programs)), encoding='utf-8')
        return flow.run(self.prior, self.isa, self.root, self.output, self.report)

    def test_verified_input_emits_readable_and_json_artifacts_with_readback_hashes(self):
        result = self.run_artifact()
        self.assertEqual(result['pairs'], 2)
        document = json.loads((self.output / 'one.flow.json').read_bytes())
        self.assertEqual(document['directControlFlow']['exits'][0]['index'], 1)
        self.assertFalse(document['scope']['executesVu'])
        for name, expected in result['programs'][0]['files'].items():
            self.assertEqual(flow.digest((self.output / name).read_bytes()), expected)
        self.assertEqual(json.loads(self.report.read_bytes())['pairs'], 2)

    def test_tampered_input_fails_before_output_creation(self):
        with (self.isa / 'one.isa.json').open('ab') as stream: stream.write(b' ')
        with self.assertRaisesRegex(ValueError, 'identity mismatch'): self.run_artifact()
        self.assertFalse(self.output.exists())
        self.assertFalse(self.report.exists())

    def test_windows_case_collisions_fail_before_output_creation(self):
        self.add_program('ONE.isa.json')
        with self.assertRaisesRegex(ValueError, 'distinct output stems'): self.run_artifact()
        self.assertFalse(self.output.exists())

    def test_existing_output_is_preserved(self):
        self.output.mkdir(parents=True)
        marker = self.output / 'keep'
        marker.write_bytes(b'evidence')
        with self.assertRaisesRegex(ValueError, 'fresh output'): self.run_artifact()
        self.assertEqual(marker.read_bytes(), b'evidence')
        self.assertFalse(self.report.exists())


if __name__ == '__main__': unittest.main()
