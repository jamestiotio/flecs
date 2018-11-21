#ifndef REFLECS_TYPES_PRIVATE_H
#define REFLECS_TYPES_PRIVATE_H

#include <stdlib.h>
#include <pthread.h>

#include <reflecs/reflecs.h>
#include <reflecs/util/array.h>
#include <reflecs/util/map.h>

#define ECS_WORLD_INITIAL_TABLE_COUNT (2)
#define ECS_WORLD_INITIAL_ENTITY_COUNT (2)
#define ECS_WORLD_INITIAL_STAGING_COUNT (0)
#define ECS_WORLD_INITIAL_PERIODIC_SYSTEM_COUNT (1)
#define ECS_WORLD_INITIAL_OTHER_SYSTEM_COUNT (0)
#define ECS_WORLD_INITIAL_INIT_SYSTEM_COUNT (0)
#define ECS_WORLD_INITIAL_DEINIT_SYSTEM_COUNT (0)
#define ECS_WORLD_INITIAL_SET_SYSTEM_COUNT (0)
#define ECS_WORLD_INITIAL_PREFAB_COUNT (0)
#define ECS_MAP_INITIAL_NODE_COUNT (4)
#define ECS_TABLE_INITIAL_ROW_COUNT (0)
#define ECS_SYSTEM_INITIAL_TABLE_COUNT (0)
#define ECS_MAX_JOBS_PER_WORKER (16)

#define ECS_WORLD_MAGIC (0x65637377)
#define ECS_THREAD_MAGIC (0x65637374)

/** A family identifies a set of components */
typedef uint32_t EcsFamily;

/* -- Builtin component types -- */

typedef struct EcsFamilyComponent {
    EcsFamily family;    /* Preserved nested families */
    EcsFamily resolved;  /* Resolved nested families */
} EcsFamilyComponent;

typedef struct EcsComponent {
    uint32_t size;
} EcsComponent;

typedef enum EcsSystemExprElemKind {
    EcsFromEntity,
    EcsFromComponent,
    EcsFromSystem
} EcsSystemExprElemKind;

typedef enum EcsSystemExprOperKind {
    EcsOperAnd = 0,
    EcsOperOr = 1,
    EcsOperNot = 2,
    EcsOperOptional = 3,
    EcsOperLast = 4
} EcsSystemExprOperKind;

typedef EcsResult (*ecs_parse_action)(
    EcsWorld *world,
    EcsSystemExprElemKind elem_kind,
    EcsSystemExprOperKind oper_kind,
    const char *component,
    void *ctx);

typedef struct EcsSystemColumn {
    EcsSystemExprElemKind kind;       /* Element kind (Entity, Component) */
    EcsSystemExprOperKind oper_kind;  /* Operator kind (AND, OR, NOT) */
    union {
        EcsFamily family;             /* Used for OR operator */
        EcsHandle component;          /* Used for AND operator */
    } is;
} EcsSystemColumn;

typedef struct EcsSystemRef {
    EcsHandle entity;
    EcsHandle component;
} EcsSystemRef;

typedef struct EcsTableSystem {
    EcsSystemAction action;    /* Callback to be invoked for matching rows */
    float period;              /* Minimum period inbetween system invocations */
    float time_passed;         /* Time passed since last invocation */
    EcsArray *columns;         /* Column components (AND) and families (OR) */
    EcsArray *components;      /* Computed component list per matched table */
    EcsArray *inactive_tables; /* Inactive tables */
    EcsArray *jobs;            /* Jobs for this system */
    EcsArray *tables;          /* Table index + refs index + column offsets */
    EcsArray *refs;            /* Columns that point to other entities */
    EcsHandle ctx_handle;      /* User-defined context for system */
    EcsArrayParams table_params; /* Parameters for tables array */
    EcsArrayParams component_params; /* Parameters for components array */
    EcsArrayParams ref_params; /* Parameters for tables array */
    EcsFamily not_from_entity;    /* Exclude components from entity */
    EcsFamily not_from_component; /* Exclude components from components */
    EcsFamily and_from_entity; /* Used to match init / deinit notifications */
    EcsFamily and_from_system; /* Used to auto-add components to system */
    bool enabled;              /* Is system enabled or not */
} EcsTableSystem;

typedef struct EcsRowSystem {
    EcsSystemAction action;     /* Callback to be invoked for matching rows */
    EcsArray *components;       /* Components in order of signature */
    EcsHandle ctx_handle;
} EcsRowSystem;

/* -- Private types -- */

typedef struct EcsTable {
    EcsArray *family;             /* Reference to family_index entry */
    EcsArray *rows;               /* Rows of the table */
    EcsArray *frame_systems;      /* Frame systems matched with table */
    EcsArrayParams row_params;    /* Parameters for rows array */
    EcsFamily family_id;          /* Identifies a family in family_index */
    uint16_t *columns;            /* Column (component) sizes */
} EcsTable;

typedef struct EcsRow {
    EcsFamily family_id;          /* Identifies a family (and table) in world */
    uint32_t index;               /* Index of the entity in its table */
} EcsRow;

typedef struct EcsEntityInfo {
    EcsHandle entity;
    EcsFamily family_id;
    uint32_t index;
    EcsTable *table;
    EcsArray *rows;
} EcsEntityInfo;

typedef struct EcsStage {
    EcsMap *add_stage;            /* Entities with components to add */
    EcsMap *remove_stage;         /* Entities with components to remove */
    EcsMap *remove_merge;         /* All removed components before merge */
    EcsArray *delete_stage;       /* Deleted entities while in progress */
    EcsMap *entity_stage;         /* Entities committed while in progress */
    EcsMap *data_stage;           /* Arrays with staged component values */
    EcsMap *family_stage;         /* Families created while >1 threads running*/
    EcsArray *table_db_stage;     /* Tables created while >1 threads running */
    EcsMap *table_stage;          /* Index for table stage */
} EcsStage;

typedef struct EcsJob {
    EcsHandle system;             /* System handle */
    EcsTableSystem *system_data;  /* System to run */
    uint32_t table_index;         /* Current SystemTable */
    uint32_t start_index;         /* Start index in row chunk */
    uint32_t row_count;           /* Total number of rows to process */
} EcsJob;

typedef struct EcsThread {
    uint32_t magic;               /* Magic number to verify thread pointer */
    uint32_t job_count;           /* Number of jobs scheduled for thread */
    EcsWorld *world;              /* Reference to world */
    EcsJob *jobs[ECS_MAX_JOBS_PER_WORKER]; /* Array with jobs */
    EcsStage *stage;              /* Stage for thread */
    pthread_t thread;             /* Thread handle */
} EcsThread;

struct EcsWorld {
    uint32_t magic;               /* Magic number to verify world pointer */

    float delta_time;             /* Time passed to ecs_progress */

    void *context;                /* Application context */

    EcsArray *table_db;           /* Table storage */
    EcsArray *frame_systems;      /* Frame systems */
    EcsArray *inactive_systems;   /* Frame systems with empty tables */
    EcsArray *on_demand_systems;  /* On demand systems */

    EcsMap *add_systems;          /* Systems invoked on ecs_add */
    EcsMap *remove_systems;       /* Systems invoked on ecs_remove */
    EcsMap *set_systems;          /* Systems invoked on ecs_set */

    EcsMap *entity_index;         /* Maps entity handle to EcsRow  */
    EcsMap *table_index;          /* Identifies a table by family_id */
    EcsMap *family_index;         /* References to component families */
    EcsMap *family_handles;       /* Index to explicitly created families */
    EcsMap *prefab_index;         /* Index for finding prefabs in families */

    EcsStage stage;              /* Stage of main thread */

    EcsArray *worker_threads;     /* Worker threads */
    EcsArray *stage_db;           /* Stage storage (one for each worker) */
    pthread_cond_t thread_cond;   /* Signal that worker threads can start */
    pthread_mutex_t thread_mutex; /* Mutex for thread condition */
    pthread_cond_t job_cond;      /* Signal that worker thread job is done */
    pthread_mutex_t job_mutex;    /* Mutex for protecting job counter */
    uint32_t jobs_finished;       /* Number of jobs finished */
    uint32_t threads_running;     /* Number of threads running */

    EcsHandle last_handle;        /* Last issued handle */
    EcsHandle deinit_table_system; /* Handle to internal deinit system */
    EcsHandle deinit_row_system;  /* Handle to internal deinit system */

    EcsFamily component_family;   /* EcsComponent, EcsId */
    EcsFamily table_system_family; /* EcsTableSystem, EcsId */
    EcsFamily row_system_family;  /* EcsRowSystem, EcsId */
    EcsFamily family_family;      /* EcsFamily, EcsId */
    EcsFamily prefab_family;      /* EcsPrefab, EcsId */

    uint32_t tick;                /* Number of computed frames by world */
    bool valid_schedule;          /* Is job schedule still valid */
    bool quit_workers;            /* Signals worker threads to quit */
    bool in_progress;             /* Is world being progressed */
    bool is_merging;              /* Is world currently being merged */
    bool auto_merge;              /* Are stages auto-merged by ecs_progress */
};

extern const EcsArrayParams handle_arr_params;
extern const EcsArrayParams stage_arr_params;
extern const EcsArrayParams table_arr_params;
extern const EcsArrayParams thread_arr_params;
extern const EcsArrayParams job_arr_params;
extern const EcsArrayParams column_arr_params;


#endif
