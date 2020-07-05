#include "sys/types.h"
#include "sys/panic.h"
#include "sys/debug.h"

#define UBSAN_ABORT     1
//#undef UBSAN_ABORT

struct source_location {
    const char *file;
    uint32_t line;
    uint32_t column;
};

struct type_descriptor {
    uint16_t kind;
    uint16_t info;
    char name[];
};

#define is_aligned(value, alignment) !(value & (alignment - 1))

#if defined(UBSAN_ABORT)
#define __ubsan_abort__     __attribute__((noreturn))
#else
#define __ubsan_abort__
#endif

static __ubsan_abort__ void ubsan_abort(struct source_location *loc, const char *error) {
    kfatal("%s:%d: %s\n", loc->file, loc->line, error);
#if defined(UBSAN_ABORT)
    panic("Undefined behavior detected\n");
#endif
}

const char *Type_Check_Kinds[] = {
    "load of",
    "store to",
    "reference binding to",
    "member access within",
    "member call on",
    "constructor call on",
    "downcast of",
    "downcast of",
    "upcast of",
    "cast to virtual base of",
};

// Out of bounds
struct out_of_bounds_info {
    struct source_location location;
    struct type_descriptor left_type;
    struct type_descriptor right_type;
};
void __ubsan_handle_out_of_bounds(struct out_of_bounds_info *out_of_bounds,
                                  uintptr_t pointer) {
    ubsan_abort(&out_of_bounds->location, "out of bounds");
}

// Pointer overflow
struct pointer_overflow_info {
    struct source_location location;
};
void __ubsan_handle_pointer_overflow(struct pointer_overflow_info *pointer_overflow,
                                     void *base_raw,
                                     void *result_raw) {
    ubsan_abort(&pointer_overflow->location, "pointer overflow");
}

// Overflows
struct overflow_info {
	struct source_location location;
	struct type_descriptor *type;
};
// Subtraction overflow
void __ubsan_handle_sub_overflow(struct overflow_info *overflow,
                                 void *lhs_raw,
                                 void *rhs_raw) {
    ubsan_abort(&overflow->location, "subtraction overflow");
}

void __ubsan_handle_add_overflow(struct overflow_info *overflow,
                                 void *lhs_raw,
                                 void *rhs_raw) {
    ubsan_abort(&overflow->location, "addition overflow");
}

void __ubsan_handle_divrem_overflow(struct overflow_info *overflow,
                                    void *lhs_raw,
                                    void *rhs_raw) {
    ubsan_abort(&overflow->location, "div/rem overflow");
}

void __ubsan_handle_mul_overflow(struct overflow_info *overflow,
                                 void *lhs_raw,
                                 void *rhs_raw) {
    ubsan_abort(&overflow->location, "multiplication overflow");
}

void __ubsan_handle_negate_overflow(struct overflow_info *overflow,
                                    void *lhs_raw) {
    ubsan_abort(&overflow->location, "negation overflow");
}

// Shift out of bounds
struct shift_out_of_bounds_info {
	struct source_location location;
	struct type_descriptor *lhs_type;
	struct type_descriptor *rhs_type;
};
void __ubsan_handle_shift_out_of_bounds(struct shift_out_of_bounds_info *shift_out_of_bounds,
                                        void *lhs_raw,
                                        void *rhs_raw) {
    struct source_location *location = &shift_out_of_bounds->location;
    ubsan_abort(location, "shift out of bounds");
}

void __ubsan_handle_vla_bound_not_positive(void *data_raw,
                                           void *bound_raw) {
    panic("VLA bound is <= 0!\n");
}

// Type mismatch
struct type_mismatch_info {
    struct source_location location;
    struct type_descriptor *type;
    uint8_t alignment;
    uint8_t type_check_kind;
};
void __ubsan_handle_type_mismatch_v1(struct type_mismatch_info *type_mismatch,
                                     uintptr_t pointer) {
    struct source_location *location = &type_mismatch->location;

    if (pointer == 0) {
        kfatal("NULL pointer access\n");
    } else if (type_mismatch->alignment != 0 &&
               is_aligned(pointer, type_mismatch->alignment)) {
        // Most useful on architectures with stricter memory alignment requirements, like ARM.
        kfatal("Unaligned memory access: %p\n", pointer);
    } else {
        //kfatal("Insufficient size:\n");
        if (type_mismatch->type_check_kind < sizeof(Type_Check_Kinds) / sizeof(Type_Check_Kinds[0])) {
            kfatal("%s address %p with insufficient space for object of type %s\n",
                Type_Check_Kinds[type_mismatch->type_check_kind], (void *) pointer,
                type_mismatch->type->name);
        }
    }

    ubsan_abort(location, "type mismatch");
}
