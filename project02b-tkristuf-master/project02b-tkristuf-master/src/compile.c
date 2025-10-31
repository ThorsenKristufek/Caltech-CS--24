#include "compile.h"

#include <stdio.h>
#include <stdlib.h>

#define BAD_TYPES_SIZE 3
#define VAR_OFFSET -8

bool constant_tree_found(node_t *node) {
    if (node->type == NUM) {
        return true;
    }
    else if (node->type == BINARY_OP) {
        binary_node_t *bi_node = (binary_node_t *) node;
        int64_t bad_types[BAD_TYPES_SIZE] = {'>', '=', '<'};
        for (size_t i = 0; i < BAD_TYPES_SIZE; i++) {
            if (bi_node->op == bad_types[i]) {
                return false;
            }
        }
        return constant_tree_found(bi_node->left) && constant_tree_found(bi_node->right);
    }
    return false;
}

value_t evaluation_tree(node_t *node) {
    if (node->type == BINARY_OP) {
        binary_node_t *bi_node = (binary_node_t *) node;
        switch (bi_node->op) {
            case '+':
                return evaluation_tree(bi_node->left) + evaluation_tree(bi_node->right);
            case '-':
                return evaluation_tree(bi_node->left) - evaluation_tree(bi_node->right);
            case '*':
                return evaluation_tree(bi_node->left) * evaluation_tree(bi_node->right);
            case '/':
                return evaluation_tree(bi_node->left) / evaluation_tree(bi_node->right);
        }
    }
    else if (node->type == NUM) {
        return ((num_node_t *) node)->value;
    }
    return 1;
}

bool compile_ast(node_t *node) {
    if (!node) return false;

    static int if_label_count = 0;
    static int while_count_l = 0;

    switch (node->type) {
        case NUM:
            printf("movq $%ld, %%rdi\n", ((num_node_t *) node)->value);
            return true;
        case PRINT:
            if (!compile_ast(((print_node_t *) node)->expr)) return false;
            printf("callq print_int\n");
            return true;
        case SEQUENCE:
            for (size_t i = 0; i < ((sequence_node_t *) node)->statement_count; i++) {
                if (!compile_ast(((sequence_node_t *) node)->statements[i])) return false;
            }
            return true;
        case BINARY_OP: {
            binary_node_t *binary_node = (binary_node_t *) node;
            if (constant_tree_found(node)) {
                printf("movq $%ld, %%rdi\n", evaluation_tree(node));
                return true;
            }
            if (!compile_ast(binary_node->right)) return false;
            printf("push %%rdi\n");
            if (!compile_ast(binary_node->left)) return false;
            printf("pop %%rsi\n");
            switch (binary_node->op) {
                case '+':
                    printf("addq %%rsi, %%rdi\n");
                    return true;
                case '-':
                    printf("subq %%rsi, %%rdi\n");
                    return true;
                case '*':
                    printf("imulq %%rsi, %%rdi\n");
                    return true;
                case '/':
                    printf("movq %%rdi, %%rax\n");
                    printf("cqto\n"); // Sign-extend rax into rdx:rax
                    printf("idivq %%rsi\n");
                    printf("movq %%rax, %%rdi\n");
                    return true;
                default:
                    printf("cmp %%rsi, %%rdi\n");
                    return true;
            }
            return true;
        }
        case VAR:
            printf("movq %d(%%rbp), %%rdi\n",
                   VAR_OFFSET * (((var_node_t *) node)->name - 'A' + 1));
            return true;
        case LET:
            if (!compile_ast(((let_node_t *) node)->value)) return false;
            printf("movq %%rdi, %d(%%rbp)\n",
                   VAR_OFFSET * (((let_node_t *) node)->var - 'A' + 1));
            return true;
        case IF: {
            if_node_t *if_node = (if_node_t *) node;
            if_label_count++;
            int if_cur = if_label_count;
            compile_ast((node_t *) if_node->condition);
            if (((binary_node_t *) if_node->condition)->op == '=') {
                printf("je IF_%d\n", if_cur);
            }
            else if (((binary_node_t *) if_node->condition)->op == '<') {
                printf("jl IF_%d\n", if_cur);
            }
            else if (((binary_node_t *) if_node->condition)->op == '>') {
                printf("jg IF_%d\n", if_cur);
            }
            printf("ELSE_%d:\n", if_cur);
            if (if_node->else_branch) {
                compile_ast((node_t *) if_node->else_branch);
            }
            printf("jmp END_IF_%d\n", if_cur);
            printf("IF_%d:\n", if_cur);
            compile_ast(if_node->if_branch);
            printf("jmp END_IF_%d\n", if_cur);
            printf("END_IF_%d:\n", if_cur);
            return true;
        }
        case WHILE: {
            while_count_l++;
            while_node_t *while_node = (while_node_t *) node;
            int while_count = while_count_l;
            printf("WHILE_%d:\n", while_count);
            compile_ast((node_t *) while_node->condition);
            if (((binary_node_t *) while_node->condition)->op == '=') {
                printf("jne WHILE_END_%d\n", while_count);
            }
            else if (((binary_node_t *) while_node->condition)->op == '<') {
                printf("jge WHILE_END_%d\n", while_count);
            }
            else if (((binary_node_t *) while_node->condition)->op == '>') {
                printf("jle WHILE_END_%d\n", while_count);
            }
            compile_ast(while_node->body);
            printf("jmp WHILE_%d\n", while_count);
            printf("WHILE_END_%d:\n", while_count);
            return true;
        }
        default:
            fprintf(stderr, "Unknown node type\n");
            return false;
    }
}
