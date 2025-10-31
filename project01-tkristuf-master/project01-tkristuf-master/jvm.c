#include "jvm.h"

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "heap.h"
#include "read_class.h"

const u1 ILOAD_OFFSET = 0x1a;
const u1 ISTORE_OFFSET = 0x3b;

/** The name of the method to invoke to run the class file */
const char MAIN_METHOD[] = "main";
/**
 * The "descriptor" string for main(). The descriptor encodes main()'s signature,
 * i.e. main() takes a String[] and returns void.
 * If you're interested, the descriptor string is explained at
 * https://docs.oracle.com/javase/specs/jvms/se12/html/jvms-4.html#jvms-4.3.2.
 */
const char MAIN_DESCRIPTOR[] = "([Ljava/lang/String;)V";

/**
 * Represents the return value of a Java method: either void or an int or a reference.
 * For simplification, we represent a reference as an index into a heap-allocated array.
 * (In a real JVM, methods could also return object references or other primitives.)
 */
typedef struct {
    /** Whether this returned value is an int */
    bool has_value;
    /** The returned value (only valid if `has_value` is true) */
    int32_t value;
} optional_value_t;

/**
 * Runs a method's instructions until the method returns.
 *
 * @param method the method to run
 * @param locals the array of local variables, including the method parameters.
 *   Except for parameters, the locals are uninitialized.
 * @param class the class file the method belongs to
 * @param heap an array of heap-allocated pointers, useful for references
 * @return an optional int containing the method's return value
 */
optional_value_t execute(method_t *method, int32_t *locals, class_file_t *class,
                         heap_t *heap) {
    u1 sipush_b1;
    u1 sipush_b2;
    code_t meth_code = method->code;
    optional_value_t result = {.has_value = false};
    int operand_stack[meth_code.max_stack];
    memset(operand_stack, 0, sizeof(int) * meth_code.max_stack);
    int stack_pointer = 0;
    int counter = 0;
    bool ret_flag = false;
    while (counter < (int) meth_code.code_length && !ret_flag) {
        u1 opcode = meth_code.code[counter];
        switch (opcode) {
            case i_iconst_m1:
            case i_iconst_0:
            case i_iconst_1:
            case i_iconst_2:
            case i_iconst_3:
            case i_iconst_4:
            case i_iconst_5:
                operand_stack[stack_pointer++] = opcode - 0x03;
                counter++;
                break;
            case i_bipush:
                counter++;
                int8_t byte_val = (int8_t) meth_code.code[counter];
                operand_stack[stack_pointer++] = (int) byte_val;
                counter++;
                break;

            case i_sipush: {
                sipush_b1 = meth_code.code[counter + 1];
                sipush_b2 = meth_code.code[counter + 2];
                int16_t val = (((int16_t) sipush_b1) << 8) | ((int16_t) sipush_b2);
                operand_stack[stack_pointer++] = val;
                counter += 3;
            } break;
            case i_iadd: {
                counter++;
                if (stack_pointer >= 2) {
                    operand_stack[stack_pointer - 2] = operand_stack[stack_pointer - 1] +
                                                       operand_stack[stack_pointer - 2];
                    stack_pointer--;
                }

            } break;
            case i_isub: {
                counter++;
                if (stack_pointer >= 2) {
                    operand_stack[stack_pointer - 2] = operand_stack[stack_pointer - 2] -
                                                       operand_stack[stack_pointer - 1];
                    stack_pointer--;
                }
            } break;

            case i_imul: {
                counter++;
                if (stack_pointer >= 2) {
                    operand_stack[stack_pointer - 2] = operand_stack[stack_pointer - 1] *
                                                       operand_stack[stack_pointer - 2];
                    stack_pointer--;
                }
            } break;

            case i_idiv: {
                counter++;
                if (stack_pointer >= 2) {
                    operand_stack[stack_pointer - 2] = operand_stack[stack_pointer - 2] /
                                                       operand_stack[stack_pointer - 1];
                    stack_pointer--;
                }
            } break;

            case i_irem: {
                counter++;
                if (stack_pointer >= 2) {
                    operand_stack[stack_pointer - 2] = operand_stack[stack_pointer - 2] %
                                                       operand_stack[stack_pointer - 1];
                    stack_pointer--;
                }
            } break;

            case i_ineg: {
                counter++;
                if (stack_pointer >= 1) {
                    operand_stack[stack_pointer - 1] = -operand_stack[stack_pointer - 1];
                }
            } break;

            case i_ishl: {
                counter++;
                if (stack_pointer >= 2) {
                    operand_stack[stack_pointer - 2] =
                        operand_stack[stack_pointer - 2]
                        << operand_stack[stack_pointer - 1];
                    stack_pointer--;
                }
            } break;

            case i_ishr: {
                counter++;
                if (stack_pointer >= 2) {
                    operand_stack[stack_pointer - 2] = operand_stack[stack_pointer - 2] >>
                                                       operand_stack[stack_pointer - 1];
                    stack_pointer--;
                }
            } break;

            case i_iushr: {
                counter++;
                if (stack_pointer >= 2) {
                    operand_stack[stack_pointer - 2] =
                        (int) ((u4) operand_stack[stack_pointer - 2] >>
                               operand_stack[stack_pointer - 1]);
                    stack_pointer--;
                }
            } break;

            case i_iand: {
                counter++;
                if (stack_pointer >= 2) {
                    operand_stack[stack_pointer - 2] =
                        (int) operand_stack[stack_pointer - 2] &
                        operand_stack[stack_pointer - 1];
                    stack_pointer--;
                }
            } break;

            case i_ior: {
                counter++;
                if (stack_pointer >= 2) {
                    operand_stack[stack_pointer - 2] =
                        (int) operand_stack[stack_pointer - 2] |
                        operand_stack[stack_pointer - 1];
                    stack_pointer--;
                }
            } break;

            case i_ixor: {
                counter++;
                if (stack_pointer >= 2) {
                    operand_stack[stack_pointer - 2] =
                        (int) operand_stack[stack_pointer - 2] ^
                        operand_stack[stack_pointer - 1];
                    stack_pointer--;
                }
            } break;

            case i_getstatic:
                counter += 3;
                break;
            case i_return:
                ret_flag = true;
                break;
            case i_invokevirtual: {
                printf("%d\n", (int) operand_stack[stack_pointer - 1]);
                stack_pointer--;
                counter += 3;
            } break;
            case i_iload: {
                counter++;
                operand_stack[stack_pointer] = locals[meth_code.code[counter]];
                counter++;
                stack_pointer++;
            } break;

            case i_istore: {
                counter++;
                if (stack_pointer > 0) {
                    locals[meth_code.code[counter]] = operand_stack[stack_pointer - 1];
                    counter++;
                    stack_pointer--;
                }
            } break;

            case i_iinc: {
                counter++;
                locals[meth_code.code[counter]] += (int8_t) meth_code.code[counter + 1];
                counter += 2;
            } break;

            case i_iload_0:
            case i_iload_1:
            case i_iload_2:
            case i_iload_3: {
                operand_stack[stack_pointer] = locals[opcode - ILOAD_OFFSET];
                counter++;
                stack_pointer++;
            } break;

            case i_istore_0:
            case i_istore_1:
            case i_istore_2:
            case i_istore_3: {
                if (stack_pointer >= 1) {
                    locals[meth_code.code[counter] - ISTORE_OFFSET] =
                        operand_stack[stack_pointer - 1];
                    stack_pointer--;
                }
                counter++;
            } break;
            case i_ldc: {
                counter++;
                u1 index = meth_code.code[counter];
                operand_stack[stack_pointer] =
                    ((CONSTANT_Integer_info *) (class->constant_pool[index - 1].info))
                        ->bytes;
                counter++;
                stack_pointer++;
            } break;

            case i_ifeq: {
                int16_t offset =
                    (meth_code.code[counter + 1] << 8) | meth_code.code[counter + 2];
                if (operand_stack[--stack_pointer] == 0) {
                    counter += offset;
                }
                else {
                    counter += 3;
                }
            } break;

            case i_ifne: {
                int16_t offset =
                    (meth_code.code[counter + 1] << 8) | meth_code.code[counter + 2];
                if (operand_stack[--stack_pointer] != 0) {
                    counter += offset;
                }
                else {
                    counter += 3;
                }
            } break;

            case i_iflt: {
                int16_t offset =
                    (meth_code.code[counter + 1] << 8) | meth_code.code[counter + 2];
                if (operand_stack[--stack_pointer] < 0) {
                    counter += offset;
                }
                else {
                    counter += 3;
                }
            } break;

            case i_ifge: {
                int16_t offset =
                    (meth_code.code[counter + 1] << 8) | meth_code.code[counter + 2];
                if (operand_stack[--stack_pointer] >= 0) {
                    counter += offset;
                }
                else {
                    counter += 3;
                }
            } break;

            case i_ifgt: {
                int16_t offset =
                    (meth_code.code[counter + 1] << 8) | meth_code.code[counter + 2];
                if (operand_stack[--stack_pointer] > 0) {
                    counter += offset;
                }
                else {
                    counter += 3;
                }
            } break;

            case i_ifle: {
                int16_t offset =
                    (meth_code.code[counter + 1] << 8) | meth_code.code[counter + 2];
                if (operand_stack[--stack_pointer] <= 0) {
                    counter += offset;
                }
                else {
                    counter += 3;
                }
            } break;

            case i_if_icmpeq: {
                int16_t offset =
                    (meth_code.code[counter + 1] << 8) | meth_code.code[counter + 2];
                int val2 = operand_stack[--stack_pointer];
                int val1 = operand_stack[--stack_pointer];
                if (val1 == val2) {
                    counter += offset;
                }
                else {
                    counter += 3;
                }
            } break;

            case i_if_icmpne: {
                int16_t offset =
                    (meth_code.code[counter + 1] << 8) | meth_code.code[counter + 2];
                int val2 = operand_stack[--stack_pointer];
                int val1 = operand_stack[--stack_pointer];
                if (val1 != val2) {
                    counter += offset;
                }
                else {
                    counter += 3;
                }
            } break;

            case i_if_icmplt: {
                int16_t offset =
                    (meth_code.code[counter + 1] << 8) | meth_code.code[counter + 2];
                int val2 = operand_stack[--stack_pointer];
                int val1 = operand_stack[--stack_pointer];
                if (val1 < val2) {
                    counter += offset;
                }
                else {
                    counter += 3;
                }
            } break;

            case i_if_icmpge: {
                int16_t offset =
                    (meth_code.code[counter + 1] << 8) | meth_code.code[counter + 2];
                int val2 = operand_stack[--stack_pointer];
                int val1 = operand_stack[--stack_pointer];
                if (val1 >= val2) {
                    counter += offset;
                }
                else {
                    counter += 3;
                }
            } break;

            case i_if_icmpgt: {
                int16_t offset =
                    (meth_code.code[counter + 1] << 8) | meth_code.code[counter + 2];
                int val2 = operand_stack[--stack_pointer];
                int val1 = operand_stack[--stack_pointer];
                if (val1 > val2) {
                    counter += offset;
                }
                else {
                    counter += 3;
                }
            } break;

            case i_if_icmple: {
                int16_t offset =
                    (meth_code.code[counter + 1] << 8) | meth_code.code[counter + 2];
                int val2 = operand_stack[--stack_pointer];
                int val1 = operand_stack[--stack_pointer];
                if (val1 <= val2) {
                    counter += offset;
                }
                else {
                    counter += 3;
                }
            } break;

            case i_goto: {
                int16_t offset =
                    (meth_code.code[counter + 1] << 8) | meth_code.code[counter + 2];
                counter += offset;
            } break;

            case i_invokestatic: {
                int16_t methodIndex = (int16_t)(meth_code.code[counter + 1] << 8) |
                                      meth_code.code[counter + 2];
                method_t *methodToCall = find_method_from_index(methodIndex, class);
                u2 numArgs = get_number_of_parameters(methodToCall);
                int32_t *calleeLocals =
                    calloc(methodToCall->code.max_locals, sizeof(int32_t));
                for (int32_t i = numArgs - 1; i >= 0; i--) {
                    stack_pointer--;
                    calleeLocals[i] = operand_stack[stack_pointer];
                }
                optional_value_t results =
                    execute(methodToCall, calleeLocals, class, heap);
                if (results.has_value) {
                    operand_stack[stack_pointer++] = results.value;
                }
                free(calleeLocals);
                counter += 3;
                break;
            }

            case i_ireturn: {
                counter++;
                stack_pointer--;
                return (optional_value_t){.has_value = true,
                                          .value = operand_stack[stack_pointer]};
                break;
            }

            case i_nop:
                counter++;
                break;

            case i_dup:
                operand_stack[stack_pointer] = operand_stack[stack_pointer - 1];
                stack_pointer++;
                counter++;
                break;

            case i_newarray: {
                if (operand_stack[stack_pointer - 1] + 1 > 0) {
                    int32_t *iarray =
                        calloc(operand_stack[stack_pointer - 1] + 1, sizeof(int32_t));
                    iarray[0] = operand_stack[stack_pointer - 1];
                    operand_stack[stack_pointer - 1] = heap_add(heap, iarray);
                    counter += 2;
                }
            } break;

            case i_arraylength:
                operand_stack[stack_pointer - 1] =
                    heap_get(heap, operand_stack[stack_pointer - 1])[0];
                counter++;
                break;

            case i_areturn:
                stack_pointer--;
                return (optional_value_t){.has_value = true,
                                          .value = operand_stack[stack_pointer]};

            case i_iastore: {
                int32_t val = operand_stack[--stack_pointer];
                int32_t index = operand_stack[--stack_pointer];
                int32_t refe = operand_stack[--stack_pointer];
                heap_get(heap, refe)[index + 1] = val;
                counter++;
            } break;

            case i_iaload: {
                int32_t index = operand_stack[--stack_pointer];
                int32_t refe = operand_stack[--stack_pointer];
                operand_stack[stack_pointer++] = heap_get(heap, refe)[index + 1];
                counter++;
            } break;

            case i_aload:
                operand_stack[stack_pointer++] = locals[meth_code.code[counter + 1]];
                counter += 2;
                break;

            case i_astore:
                stack_pointer--;
                locals[meth_code.code[counter + 1]] = operand_stack[stack_pointer];
                counter += 2;
                break;

            case i_aload_0:
            case i_aload_1:
            case i_aload_2:
            case i_aload_3:
                operand_stack[stack_pointer++] = locals[opcode - i_aload_0];
                counter++;
                break;

            case i_astore_0:
            case i_astore_1:
            case i_astore_2:
            case i_astore_3:
                stack_pointer--;
                locals[opcode - i_astore_0] = operand_stack[stack_pointer];
                counter++;
                break;
            default:
                break;
        }
    }
    return result;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "USAGE: %s <class file>\n", argv[0]);
        return 1;
    }

    // Open the class file for reading
    FILE *class_file = fopen(argv[1], "r");
    assert(class_file != NULL && "Failed to open file");

    // Parse the class file
    class_file_t *class = get_class(class_file);
    int error = fclose(class_file);
    assert(error == 0 && "Failed to close file");

    // The heap array is initially allocated to hold zero elements.
    heap_t *heap = heap_init();

    // Execute the main method
    method_t *main_method = find_method(MAIN_METHOD, MAIN_DESCRIPTOR, class);
    assert(main_method != NULL && "Missing main() method");
    /* In a real JVM, locals[0] would contain a reference to String[] args.
     * But since TeenyJVM doesn't support Objects, we leave it uninitialized. */
    int32_t locals[main_method->code.max_locals];
    // Initialize all local variables to 0
    memset(locals, 0, sizeof(locals));
    optional_value_t result = execute(main_method, locals, class, heap);
    assert(!result.has_value && "main() should return void");

    // Free the internal data structures
    free_class(class);

    // Free the heap
    heap_free(heap);
}
