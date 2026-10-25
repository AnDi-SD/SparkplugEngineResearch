# Host adapters for recovered source

These are our explicit policies and lifetime bridges. They do not reconstruct
native allocation, ABI or execution merely by adapting recovered classes.

`VisibilityGraphSnapshot` captures actual source-class partition memberships,
current world bounds and marks for one synchronous Visibility/Scene CPU call.
Call the actual world/model producers first, build one fresh snapshot, execute
the concrete traversal and Scene occlusion, then publish marks. The snapshot
must outlive all Manager output uses; source objects and graph links remain
alive and stable throughout. Source ownership and native Scene registration
are not created by this bridge. Failed world updates must be handled by the
caller before capture.

`LegacySortPolicy` is an explicit versioned host sort choice. Scene cohorts
larger than 32 and the general renderer queue have native compiler sorts;
their equal-key order is not established by choosing the CRT adapter.
