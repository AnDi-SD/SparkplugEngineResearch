"""Accounting checks for rejection, irreversible API errors, and late faults."""
from copy import deepcopy
import json
from analyze_selected_submit import summarize


def run():
    init = dict(event='init', schema=1, enabled=True)
    frame = dict(event='frame', frame=10, attempts=3, calls=2, instances=2,
                 rejected=1, apiFailures=0, reentries=0, postCommitFaults=0,
                 disabledAfterFailure=False)
    fault = dict(frame, frame=11, attempts=1, calls=1, instances=1, rejected=0,
                 postCommitFaults=1, disabledAfterFailure=True)
    fallback = dict(frame, frame=12, attempts=0, calls=0, instances=0, rejected=0,
                    disabledAfterFailure=True)
    result = summarize([init, frame, fault, fallback])
    assert result['totals']['instances'] == 3 and result['totals']['postCommitFaults'] == 1
    assert result['firstDisabledFrame'] == 11
    checks = 1
    error = dict(fault, instances=0, apiFailures=1, postCommitFaults=0)
    result = summarize([init, frame, error, fallback])
    assert result['totals']['calls'] == 3 and result['totals']['apiFailures'] == 1
    checks += 1
    for invalid in (
        [frame], [init], [init, init, frame], [init, frame, frame],
        [init, dict(frame, calls=1)], [init, dict(frame, instances=1)],
        [init, dict(frame, calls=True)], [init, dict(frame, rejected=-1)],
        [init, dict(fault, disabledAfterFailure=False)],
        [init, dict(error, disabledAfterFailure=False)],
        [init, fault, dict(fallback, disabledAfterFailure=False)],
        [init, dict(fault, postCommitFaults=2)],
        [init, dict(frame, disabledAfterFailure=0)],
    ):
        try:
            summarize(deepcopy(invalid))
        except ValueError:
            checks += 1
        else:
            raise AssertionError(f'Invalid accounting accepted: {invalid}')
    return dict(status='PASS', checks=checks, gpu=False, nativeGameCodeExecuted=False)


if __name__ == '__main__':
    print(json.dumps(run()))
