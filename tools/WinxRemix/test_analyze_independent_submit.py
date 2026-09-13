"""Accounting tests: partial success must remain visible, never become a PASS."""
from copy import deepcopy
import json
from analyze_independent_submit import COUNTERS, summarize


def run():
    checks = 0
    init = dict(event='init', schema=1, enabled=True)
    frame = dict(event='frame', frame=12, comparison=False, disabledAfterFailure=False,
                 **dict.fromkeys(COUNTERS, 0))
    frame.update(groups=2, instances=3, rejected=4)
    result = summarize([init, frame])
    assert result['totals']['instances'] == 3 and result['totals']['rejected'] == 4
    checks += 1
    partial = dict(frame, frame=13, groups=1, instances=1, apiFailures=1, disabledAfterFailure=True)
    fallback = dict(frame, frame=14, groups=0, instances=0, disabledAfterFailure=True)
    result = summarize([init, frame, partial, fallback])
    assert result['totals']['instances'] == 4 and result['totals']['apiFailures'] == 1
    assert result['firstDisabledFrame'] == 13 and result['disabledAfterFailure']
    checks += 1
    comparison = dict(frame, frame=15, groups=0, instances=0, comparison=True, disabledAfterFailure=True)
    result = summarize([init, partial, comparison])
    assert result['modes'] == {'direct': 1, 'comparison': 1}
    checks += 1
    invalid = [
        [frame], [init], [init, init, frame], [dict(init, enabled=False), frame],
        [init, frame, frame], [init, dict(frame, instances=True)],
        [init, dict(frame, rejected=-1)], [init, dict(frame, comparison=1)],
        [init, dict(frame, comparison=True)], [init, dict(frame, groups=0)],
        [init, dict(frame, suppressedDraws=1)],
        [init, partial, dict(fallback, disabledAfterFailure=False)],
        [init, dict(event='mystery', frame=12)],
    ]
    for rows in invalid:
        try:
            summarize(deepcopy(rows))
        except ValueError:
            checks += 1
        else:
            raise AssertionError(f'Invalid accounting accepted: {rows}')
    return dict(status='PASS', checks=checks, gpu=False, nativeGameCodeExecuted=False)


if __name__ == '__main__':
    print(json.dumps(run()))
