#include "test.h"
#include <meglang/ast.h>
#include <stdlib.h>
int main(void)
{
    Source source = {0};
    Program *program;
    Statement *block, *return_statement;
    Expr *integer;
    FILE *output;
    EXPECT(source_from_string(&source, "test.meg", "1"));
    program = calloc(1, sizeof *program);
    block = calloc(1, sizeof *block);
    return_statement = calloc(1, sizeof *return_statement);
    integer = calloc(1, sizeof *integer);
    EXPECT(program && block && return_statement && integer);
    if (program && block && return_statement && integer) {
        integer->kind = EXPR_INT; integer->as.integer = 1;
        integer->span = (SourceSpan){&source, 0, 1, 1, 1};
        return_statement->kind = STMT_RETURN;
        return_statement->as.expression = integer;
        block->kind = STMT_BLOCK; block->as.block.items = return_statement;
        program->source = &source; program->function.body = block;
        output = tmpfile();
        EXPECT(output != NULL);
        if (output) { EXPECT(ast_print(output, program)); fclose(output); }
        program_destroy(program);
    } else {
        free(integer); free(return_statement); free(block); free(program);
    }
    source_destroy(&source);
    return RESULT();
}
