#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// トークンの種類
// 型名を書くときにenumを省略できるようにする。
typedef enum
{
    TK_RESERVED, // 記号
    TK_NUM,      // 整数トークン
    TK_EOF,      // 入力の終わりを表すトークン
} TokenKind;

// 構造体を生成するときにstructを省略できるようにする。
typedef struct Token Token;

// トークン型
struct Token
{
    TokenKind kind; // トークンの型
    Token *next;    // 次の入力トークン
    int val;        // kindがTK_NUMの場合、その数値
    char *str;      // トークン文字(このトークン以降の文字列)
};

// 現在着目しているトークン
Token *token;

// 入力された文字列
char *user_input;

// 第1引数でこの関数を呼んだトークンの位置(token->str)を受け取る。
void error_at(char *loc, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    int pos = loc - user_input;
    fprintf(stderr, "%s\n", user_input);
    fprintf(stderr, "%*s", pos, " "); // pos個の空白を出力
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

// エラーを報告するための関数(引数なしや複数個に対応)
// printfと同じ引数を取る。
// ...は可変長引数(stdarg.h)
void error(char *fmt, ...)
{
    // 可変長引数の先頭を指すポインタ(typedef void* va_list;)
    va_list ap;
    // va_startは可変長引数になる直前の引数を指定すると、最初の可変長引数のアドレスがポインタapに入る。
    va_start(ap, fmt);
    // vfprintf(出力先のストリーム, 文字列, 可変長引数リスト)
    // fprintfの引数の数は静的に決まるがvfprintfは動的に決められる。
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

// 次のトークンが期待している記号の時には、トークンを1つ読み進めて真を返す。
// '->'は構造体のポインタが指す構造体のメンバへアクセスできる(*(構造体ポインタ型変数).メンバ名と同義)。
bool consume(char op)
{
    // token->str[0]はそのトークンの文字を指す
    if (token->kind != TK_RESERVED || token->str[0] != op)
    {
        return false;
    }
    // トークンを1つ読み進める。
    token = token->next;
    return true;
}

// 次のトークンが期待している記号の時には、トークンを1つ読み進める。
// それ以外の場合にはエラーを報告する。
void expect(char op)
{
    if (token->kind != TK_RESERVED || token->str[0] != op)
    {
        error_at(token->str, "'%c'ではありません", op);
    }
    token = token->next;
}

// 次のトークンが数値の場合、トークンを1つ読み進めてその数値を返す。
// それ以外の場合にはエラーを報告する。
int expect_number()
{
    if (token->kind != TK_NUM)
    {
        error_at(token->str, "数ではありません");
    }
    int val = token->val;
    token = token->next;
    return val;
}

// トークン列の最後かどうかの判定
bool at_eof()
{
    return token->kind == TK_EOF;
}

// 新しいトークンを生成して現在のトークン列の一番後ろ(cur)に繋げ、生成したトークンのポインタを返す。
Token *new_token(TokenKind kind, Token *cur, char *str)
{
    // callocはヒープ(プログラム中で動的に確保するメモリ領域)から第2引数で指定した大きさ(byte)のブロックを第1引数個割り当てる。
    // mallocの初期値は不定、callocはゼロクリアされる。
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->str = str;
    cur->next = tok;
    return tok;
}

// 入力文字列pをトークナイズしてそれを返す。
Token *tokenize()
{
    char *p = user_input;
    // トークン列の先頭を中身は空で作る(ダミー)。
    Token head;
    head.next = NULL;
    Token *cur = &head;

    while (*p)
    {
        // isspaceは空白文字かどうかを判定(ctype.h)
        if (isspace(*p))
        {
            p++;
            continue;
        }

        if (*p == '+' || *p == '-')
        {
            // 後置インクリメントは関数呼び出し後にインクリメントされる。
            cur = new_token(TK_RESERVED, cur, p++);
            continue;
        }

        // isdigitは引数の文字が数字かどうかを判定(ctype.h)
        if (isdigit(*p))
        {
            cur = new_token(TK_NUM, cur, p);
            // strtolは受け取った文字列の先頭から数字の部分をlong型の(第3引数)進数に変換し、残りの部分の先頭アドレスを第2引数に渡す。
            cur->val = strtol(p, &p, 10);
            continue;
        }

        error_at(p, "トークナイズできません");
    }

    // トークン列の最後を表すトークンを生成
    new_token(TK_EOF, cur, p);
    // 先頭のトークンは空なので、2番目以降を返す。
    return head.next;
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        error("%s: 引数の個数が正しくありません", argv[0]);
        return 1;
    }

    user_input = argv[1];
    token = tokenize();

    // アセンブリの前半部分を出力
    printf(".intel_syntax noprefix\n");
    printf(".globl main\n");
    printf("main:\n");

    // 式の最初は数でなければならないので、それをチェックして最初のmov命令を出力。
    printf("  mov rax, %d\n", expect_number());

    // '+ <数値>'あるいは '- <数値>'というトークン列の並びを消費しつつ、アセンブリを出力。
    while (!at_eof())
    {
        if (consume('+'))
        {
            printf("  add rax, %d\n", expect_number());
            continue;
        }

        expect('-');
        printf("  sub rax, %d\n", expect_number());
    }

    printf("  ret\n");
    return 0;
}
