/* Each node that yields a value has a type. This information can
 * probably be used by the code generator, somehow. */
typedef enum { /* value type */
     MVM_JIT_VOID,
     MVM_JIT_MEM,
     MVM_JIT_REG,
     MVM_JIT_FLAG,
     MVM_JIT_LBL,
     MVM_JIT_INT,
     MVM_JIT_NUM,
     MVM_JIT_PTR,
} MVMJitExprVtype;

#define MVM_JIT_PTR_SZ sizeof(void*)
#define MVM_JIT_REG_SZ sizeof(MVMRegister)
#define MVM_JIT_INT_SZ sizeof(MVMint64)
#define MVM_JIT_NUM_SZ sizeof(MVMnum64)



/* This defines a macro that defines a list which will use a macro to
   define a list. It's a little trick I've gained from the luajit
   source code - the big advantage of course is that it keeps the list
   consistent across multiple definitions.

   The first argument is the name, the second the number of children,
   the third the number of parameters - together they define the size
   of the node. The last argument defines the result type - which I
   vaguely presume to be useful in code generation. */

#define MVM_JIT_IR_OPS(_) \
    /* memory access */ \
    _(LOAD, 1, 1, REG), \
    _(STORE, 2, 1, VOID), \
    _(CONST, 0, 2, REG), \
    _(ADDR, 1, 1, MEM), \
    _(IDX, 2, 1, MEM), \
    _(COPY, 1, 0, REG), \
    /* type conversion */ \
    _(CONVERT, 1, 2, REG), \
    /* integer comparison */ \
    _(LT, 2, 0, FLAG), \
    _(LE, 2, 0, FLAG), \
    _(EQ, 2, 0, FLAG), \
    _(NE, 2, 0, FLAG), \
    _(GE, 2, 0, FLAG), \
    _(GT, 2, 0, FLAG), \
    _(NZ, 1, 0, FLAG), \
    _(ZR, 1, 0, FLAG), \
    /* flag value */ \
    _(FLAGVAL, 1, 0, REG), \
    /* integer arithmetic */ \
    _(ADD, 2, 0, REG), \
    _(SUB, 2, 0, REG), \
    /* binary operations */ \
    _(AND, 2, 0, REG), \
    _(OR, 2, 0, REG), \
    _(XOR, 2, 0, REG), \
    /* boolean logic */ \
    _(NOT, 1, 0, REG),  \
    _(ALL, -1, 0, FLAG), \
    _(ANY, -1, 0, FLAG), \
    /* control operators */ \
    _(DO, -1, 0, REG), \
    _(WHEN, 2, 0, VOID), \
    _(IF, 3, 0, REG), \
    _(EITHER, 3, 0, VOID), \
    _(BRANCH, 1, 0, VOID),  \
    _(LABEL, 1, 0, VOID), \
    /* special control operators */ \
     _(INVOKISH, 1, 0, VOID), \
     _(THROWISH, 1, 0, VOID), \
    /* call c functions */ \
    _(CALL, 2, 1, REG), \
    _(ARGLIST, -1, 0, VOID), \
    _(CARG, 1, 1, VOID), \
    /* interpreter special variables */ \
    _(TC, 0, 0, REG), \
    _(CU, 0, 0, MEM), \
    _(FRAME, 0, 0, MEM), \
    _(LOCAL, 0, 0, MEM), \
    _(STACK, 0, 0, MEM), \
    _(VMNULL, 0, 0, REG), \
    /* End of list marker */ \
    _(MAX_NODES, 0, 0, VOID), \



enum MVMJitExprOp {
#define MVM_JIT_IR_ENUM(name, nchild, npar, vtype) MVM_JIT_##name
MVM_JIT_IR_OPS(MVM_JIT_IR_ENUM)
#undef MVM_JIT_IR_ENUM
};

typedef MVMint64 MVMJitExprNode;

struct MVMJitExprOpInfo {
    const char     *name;
    MVMint32        nchild;
    MVMint32        nargs;
    MVMJitExprVtype vtype;
};

struct MVMJitExprValue {
    /* used to signal register allocator, tiles don't look at this */
    MVMJitExprVtype type;
    enum {
        MVM_JIT_VALUE_EMPTY,
        MVM_JIT_VALUE_ALLOCATED,
        MVM_JIT_VALUE_SPILLED,
        MVM_JIT_VALUE_DEAD
    } state;
    /* different values */
    union {
        struct {
            MVMint8 r0;
            MVMint8 r1;
            MVMint8 c;
        } mem;
        struct {
            MVMint8 cls;
            MVMint8 num;
        } reg;
        MVMint32 label;
        MVMint64 const_val;
    } u;
    MVMint8  size;

    /* Spill information if any */
    MVMint16 spill_location;

    /* Compilation information */
    MVMint32 order_nr;
    /* TODO - we really do need this, but i'm not sure how exactly it
       propagates over conditionals */
    MVMint32 reg_req;

    /* Use information - I'd may want to change this into list of uses */
    MVMint32 first_use;
    MVMint32 last_use;
    MVMint32 num_use;
};

/* Tree node information for easy access and use during compilation (a
   symbol table entry of sorts) */
struct MVMJitExprNodeInfo {
    const MVMJitExprOpInfo *op_info;
    /* VM instruction represented by this node */
    MVMSpeshIns    *spesh_ins;
    /* VM Local value of this node */
    MVMint16        local_addr;

    /* Tiler result */
    const MVMJitTile *tile;
    MVMint32          tile_state;
    MVMint32          tile_rule;

    /* internal label for IF/WHEN/ALL/ANY etc */
    MVMint32        internal_label;

    /* Result value information (register/memory location, size etc) */
    MVMJitExprValue value;
};

struct MVMJitExprTree {
    MVMJitGraph *graph;
    MVM_DYNAR_DECL(MVMJitExprNode, nodes);
    MVM_DYNAR_DECL(MVMint32, roots);
    MVM_DYNAR_DECL(MVMJitExprNodeInfo, info);
};

struct MVMJitExprTemplate {
    const MVMJitExprNode *code;
    const char *info;
    MVMint32 len;
    MVMint32 root;
    MVMint32 flags;
};

#define MVM_JIT_EXPR_TEMPLATE_VALUE       0
#define MVM_JIT_EXPR_TEMPLATE_DESTRUCTIVE 1


struct MVMJitTreeTraverser {
    void  (*preorder)(MVMThreadContext *tc, MVMJitTreeTraverser *traverser,
                      MVMJitExprTree *tree, MVMint32 node);
    void   (*inorder)(MVMThreadContext *tc, MVMJitTreeTraverser *traverser,
                      MVMJitExprTree *tree, MVMint32 node, MVMint32 child);
    void (*postorder)(MVMThreadContext *tc, MVMJitTreeTraverser *traverser,
                      MVMJitExprTree *tree, MVMint32 node);
    void       *data;
    MVM_DYNAR_DECL(MVMint32, visits);
};


const MVMJitExprOpInfo * MVM_jit_expr_op_info(MVMThreadContext *tc, MVMJitExprNode node);
MVMJitExprTree * MVM_jit_expr_tree_build(MVMThreadContext *tc, MVMJitGraph *jg,
                                         MVMSpeshBB *bb);
void MVM_jit_expr_tree_traverse(MVMThreadContext *tc, MVMJitExprTree *tree,
                                MVMJitTreeTraverser *traverser);
void MVM_jit_expr_tree_destroy(MVMThreadContext *tc, MVMJitExprTree *tree);