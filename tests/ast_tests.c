#include "test.h"
#include <meglang/ast.h>
#include <stdlib.h>
int main(void)
{
    Source source = {0};
    Program *program;
    Function *function;
    Statement *block, *return_statement;
    Expr *integer;
    FILE *output;
    EXPECT(source_from_string(&source, "test.meg", "maini64"));
    program = calloc(1, sizeof *program);
    function = calloc(1, sizeof *function);
    block = calloc(1, sizeof *block);
    return_statement = calloc(1, sizeof *return_statement);
    integer = calloc(1, sizeof *integer);
    EXPECT(program && function && block && return_statement && integer);
    if (program && function && block && return_statement && integer) {
        integer->kind = EXPR_INT; integer->as.integer = 1;
        integer->span = (SourceSpan){&source, 0, 1, 1, 1};
        return_statement->kind = STMT_RETURN;
        return_statement->as.expression = integer;
        block->kind = STMT_BLOCK; block->as.block.items = return_statement;
        program->source = &source; program->functions = function;
        function->name = (SourceSpan){&source, 0, 4, 1, 1};
        function->return_type_name = (SourceSpan){&source, 4, 3, 1, 5};
        function->body = block;
        output = tmpfile();
        EXPECT(output != NULL);
        if (output) { EXPECT(ast_print(output, program)); fclose(output); }
        program_destroy(program);
    } else {
        free(integer); free(return_statement); free(block); free(function); free(program);
    }
    source_destroy(&source);
    return RESULT();
}
