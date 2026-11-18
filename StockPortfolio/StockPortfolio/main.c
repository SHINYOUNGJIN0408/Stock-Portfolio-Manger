#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <windows.h>
#include <io.h>
#include <direct.h>

#define MAX_STOCKS 100
#define MAX_TRADES 1000
#define SYMBOL_SIZE 16
#define LINE_SIZE 128

typedef struct {
    char symbol[16];
    int quantity;
    double averagePrice;
    double currentPrice;
    double realizedProfit;
} Stock;

typedef enum { TRADE_BUY, TRADE_SELL } TradeType;
typedef struct {
    char symbol[SYMBOL_SIZE];
    TradeType type;
    int quantity;
    double price;
    double realized_profit;
} Trade;

typedef struct {
    Stock stocks[MAX_STOCKS];
    int stock_count;
    Trade trades[MAX_TRADES];
    int trade_count;
} Portfolio;

/* 한 줄을 끝까지 소비한 뒤 앞뒤 공백을 제거한다. EOF면 0을 반환한다. */
static int read_line(const char *prompt, char line[LINE_SIZE])
{
    for (;;) {
        size_t length = 0, start = 0;
        int invalid = 0, ch;
        printf("%s", prompt);
        fflush(stdout);
        while ((ch = getchar()) != '\n' && ch != EOF) {
            if (ch == 0 || length == LINE_SIZE - 1)
                invalid = 1;
            else
                line[length++] = (char)ch;
        }
        if (ch == EOF && (length == 0 || ferror(stdin)))
            return 0;
        if (!invalid) {
            while (length && isspace((unsigned char)line[length - 1]))
                --length;
            while (start < length && isspace((unsigned char)line[start]))
                ++start;
            if (start < length) {
                memmove(line, line + start, length - start);
                line[length - start] = '\0';
                return 1;
            }
        }
        puts("Invalid input. Enter a non-empty, shorter line.");
    }
}

static int read_integer(const char *prompt, int min, int max, int *value)
{
    char line[LINE_SIZE], *end;
    while (read_line(prompt, line)) {
        errno = 0;
        long n = strtol(line, &end, 10);
        if (end != line && *end == '\0' && !errno && n >= min && n <= max) {
            *value = (int)n;
            return 1;
        }
        printf("Enter a whole number from %d to %d.\n", min, max);
    }
    return 0;
}

static int read_price(double *price)
{
    char line[LINE_SIZE], *end;
    while (read_line("Price per share (USD): ", line)) {
        errno = 0;
        double n = strtod(line, &end);
        if (end != line && *end == '\0' && !errno && isfinite(n) && n > 0) {
            *price = n;
            return 1;
        }
        puts("Enter a finite price greater than zero.");
    }
    return 0;
}

/* 대소문자가 다른 같은 종목을 찾을 수 있도록 입력을 대문자로 통일한다. */
static int read_symbol(char symbol[SYMBOL_SIZE])
{
    char line[LINE_SIZE];
    while (read_line("Stock symbol: ", line)) {
        size_t len = strlen(line);
        int has_alnum = 0;
        size_t i;
        for (i = 0; i < len && i < SYMBOL_SIZE - 1; ++i) {
            unsigned char ch = (unsigned char)line[i];
            if (isalnum(ch))
                has_alnum = 1;
            else if (ch != '.' && ch != '-')
                break;
            symbol[i] = (char)toupper(ch);
        }
        if (i == len && has_alnum) {
            symbol[i] = '\0';
            return 1;
        }
        puts("Use 1-15 letters, digits, dots or hyphens, including a letter or digit.");
    }
    return 0;
}

static int find_stock(const Portfolio *p, const char *symbol)
{
    for (int i = 0; i < p->stock_count; ++i)
        if (strcmp(p->stocks[i].symbol, symbol) == 0)
            return i;
    return -1;
}

/* 두 CSV를 비활성 슬롯에 완성한 뒤 선택 파일 하나만 교체한다. */
static int active_slot = -1;
static const char *holdings_path[] = {"holdings-0.csv", "holdings-1.csv"};
static const char *trades_path[] = {"trades-0.csv", "trades-1.csv"};
#define INDEX_FILE "portfolio.index"

static int write_csv(const Portfolio *p, int slot, int trades)
{
    FILE *f;
    if (fopen_s(&f, trades ? trades_path[slot] : holdings_path[slot], "w"))
        return 0;
    int ok = 1;
    if (trades) {
        ok = fprintf(f, "symbol,type,quantity,price,realizedProfit\n") > 0;
        for (int i = 0; ok && i < p->trade_count; ++i) {
            const Trade *t = &p->trades[i];
            ok = fprintf(f, "%s,%s,%d,%.17g,%.17g\n", t->symbol,
                t->type == TRADE_BUY ? "BUY" : "SELL", t->quantity,
                t->price, t->realized_profit) > 0;
        }
    } else {
        ok = fprintf(f, "symbol,quantity,averagePrice,currentPrice,realizedProfit\n") > 0;
        for (int i = 0; ok && i < p->stock_count; ++i) {
            const Stock *s = &p->stocks[i];
            ok = fprintf(f, "%s,%d,%.17g,%.17g,%.17g\n", s->symbol,
                s->quantity, s->averagePrice, s->currentPrice, s->realizedProfit) > 0;
        }
    }
    return fclose(f) == 0 && ok;
}

/* 행 개수와 파일 지문으로 누락, 절단 및 다른 저장본과의 혼합을 확인한다. */
static int fingerprint(const char *path, unsigned *result)
{
    FILE *f;
    if (fopen_s(&f, path, "rb")) return 0;
    unsigned h = 2166136261u;
    int ch;
    while ((ch = fgetc(f)) != EOF)
        h = (h ^ (unsigned)ch) * 16777619u;
    int ok = !ferror(f);
    fclose(f);
    *result = h;
    return ok;
}

/* 최초 저장 전에 빈 상태도 원자적으로 확정해 실패 후 재시작할 수 있게 한다. */
static int mark_empty_snapshot(void)
{
    FILE *f;
    if (fopen_s(&f, INDEX_FILE ".tmp", "w")) return 0;
    int ok = fputs("EMPTY1\n", f) >= 0;
    if (fclose(f) || !ok) return 0;
    return MoveFileExW(L"portfolio.index.tmp", L"portfolio.index",
                       MOVEFILE_WRITE_THROUGH) != 0;
}

static int save_portfolio(const Portfolio *p)
{
    int slot = active_slot == 0 ? 1 : 0;
    unsigned a, b;
    FILE *f;
    if (active_slot == -1) {
        if (!mark_empty_snapshot()) return 0;
        active_slot = -2; /* 빈 스냅샷에서 첫 CSV 저장을 시도한다. */
    }
    if (!write_csv(p, slot, 0) || !write_csv(p, slot, 1)
        || !fingerprint(holdings_path[slot], &a) || !fingerprint(trades_path[slot], &b)
        || fopen_s(&f, INDEX_FILE ".tmp", "w"))
        return 0;
    int ok = fprintf(f, "SP1,%d,%d,%d,%u,%u\n", slot, p->stock_count,
                     p->trade_count, a, b) > 0;
    if (fclose(f) || !ok || !MoveFileExW(L"portfolio.index.tmp", L"portfolio.index",
                          MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        return 0;
    active_slot = slot;
    return 1;
}

static int number_field(const char *s, double *value)
{
    char *end;
    errno = 0;
    *value = strtod(s, &end);
    return end != s && !*end && !errno && isfinite(*value);
}

static int unsigned_field(const char *s, unsigned limit, unsigned *value)
{
    char *end;
    errno = 0;
    unsigned long n = strtoul(s, &end, 10);
    if (!isdigit((unsigned char)*s) || *end || errno || n > limit) return 0;
    *value = (unsigned)n;
    return 1;
}

/* 프로그램이 쓰는 고정 열 CSV 형식만 허용한다. */
static int fields(FILE *f, char *line, char **col, int count)
{
    int ch, n = 0, k = 1;
    col[0] = line;
    while ((ch = fgetc(f)) != EOF && ch != '\n') {
        if (!ch || n >= 254) return 0;
        if (ch == ',') {
            if (k == count) return 0;
            line[n++] = '\0';
            col[k++] = line + n;
        } else line[n++] = (char)ch;
    }
    line[n] = '\0';
    return !ferror(f) && k == count;
}

static int load_rows(FILE *f, Portfolio *p, unsigned rows, int trades)
{
    char buf[256], *col[5];
    const char *header = trades ? "symbol,type,quantity,price,realizedProfit\n"
        : "symbol,quantity,averagePrice,currentPrice,realizedProfit\n";
    if (!fgets(buf, sizeof buf, f) || strcmp(buf, header)) return 0;
    for (unsigned i = 0; i < rows; ++i) {
        unsigned qty;
        double price, realized;
        if (!fields(f, buf, col, 5)) return 0;
        size_t len = strlen(col[0]);
        if (!len || len >= SYMBOL_SIZE || strspn(col[0], "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-") != len
            || !strpbrk(col[0], "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789")
            || !unsigned_field(col[trades ? 2 : 1], INT_MAX, &qty)
            || !number_field(col[trades ? 3 : 2], &price)
            || !number_field(col[4], &realized)) return 0;
        int index = find_stock(p, col[0]);
        if (trades) {
            Trade *t = &p->trades[i];
            if (!qty || price <= 0 || index < 0) return 0;
            if (!strcmp(col[1], "BUY") && realized == 0) t->type = TRADE_BUY;
            else if (!strcmp(col[1], "SELL")) t->type = TRADE_SELL;
            else return 0;
            memcpy(t->symbol, col[0], len + 1);
            t->quantity = (int)qty;
            t->price = price;
            t->realized_profit = realized;
            ++p->trade_count;
        } else {
            Stock *s = &p->stocks[i];
            if (index >= 0 || (qty ? price <= 0 : price != 0)
                || !number_field(col[3], &s->currentPrice) || s->currentPrice < 0)
                return 0;
            memcpy(s->symbol, col[0], len + 1);
            s->quantity = (int)qty;
            s->averagePrice = price;
            s->realizedProfit = realized;
            ++p->stock_count;
        }
    }
    return fgetc(f) == EOF && !ferror(f);
}

static int load_portfolio(Portfolio *p)
{
    FILE *f;
    if (fopen_s(&f, INDEX_FILE, "r")) {
        if (errno != ENOENT) return 0;
        /* 선택 파일만 사라진 기존 데이터를 덮어쓰지 않는다. */
        for (int i = 0; i < 2; ++i)
            if (_access(holdings_path[i], 0) == 0 || _access(trades_path[i], 0) == 0)
                return 0;
        return 1;
    }
    int first = fgetc(f);
    if (first == 'E') {
        char tail[8];
        int empty = fgets(tail, sizeof tail, f) && !strcmp(tail, "MPTY1\n")
            && fgetc(f) == EOF && !ferror(f);
        fclose(f);
        if (empty) active_slot = -2;
        return empty;
    }
    if (first == EOF || ungetc(first, f) == EOF) {
        fclose(f);
        return 0;
    }
    unsigned slot, rows[2], hashes[2];
    char buf[256], *col[6];
    int ok = fields(f, buf, col, 6) && !strcmp(col[0], "SP1")
        && unsigned_field(col[1], 1, &slot)
        && unsigned_field(col[2], MAX_STOCKS, &rows[0])
        && unsigned_field(col[3], MAX_TRADES, &rows[1])
        && unsigned_field(col[4], UINT_MAX, &hashes[0])
        && unsigned_field(col[5], UINT_MAX, &hashes[1])
        && fgetc(f) == EOF && !ferror(f);
    fclose(f);
    if (!ok) return 0;
    Portfolio restored = {0};
    for (int i = 0; i < 2; ++i) {
        const char *path = i ? trades_path[slot] : holdings_path[slot];
        unsigned actual;
        if (!fingerprint(path, &actual) || actual != hashes[i] || fopen_s(&f, path, "r"))
            return 0;
        ok = load_rows(f, &restored, rows[i], i);
        fclose(f);
        if (!ok) return 0;
    }
    *p = restored;
    active_slot = (int)slot;
    return 1;
}

/* 용량과 수량을 먼저 검사한 뒤 보유 정보와 매수 기록을 함께 갱신한다. */
static int buy_stock(Portfolio *p, const char *symbol, int quantity, double price)
{
    int index = find_stock(p, symbol);
    if (p->trade_count == MAX_TRADES) {
        puts("Trade history is full.");
        return 0;
    }
    if (index < 0 && p->stock_count == MAX_STOCKS) {
        puts("Portfolio is full. Cannot add a new symbol.");
        return 0;
    }
    Stock *stock = &p->stocks[index < 0 ? p->stock_count : index];
    if (stock->quantity > INT_MAX - quantity) {
        puts("Total quantity is too large.");
        return 0;
    }
    Portfolio before = *p;
    int total = stock->quantity + quantity;
    if (index < 0) {
        memcpy(stock->symbol, symbol, strlen(symbol) + 1);
        stock->averagePrice = price;
        ++p->stock_count;
    } else {
        /* 가중평균을 낮은 단가에서 시작해 단가와 수량의 큰 곱을 피한다. */
        double old = stock->averagePrice;
        if (price >= old)
            stock->averagePrice = old + (price - old) * ((double)quantity / total);
        else
            stock->averagePrice = price + (old - price) * ((double)stock->quantity / total);
    }
    stock->quantity = total;
    Trade *trade = &p->trades[p->trade_count++];
    memcpy(trade->symbol, symbol, strlen(symbol) + 1);
    trade->type = TRADE_BUY;
    trade->quantity = quantity;
    trade->price = price;
    if (!save_portfolio(p)) { *p = before; puts("Save failed. Purchase cancelled."); return 0; }
    printf("Purchase complete: %s | Held: %d | Average: %.2f USD\n",
           stock->symbol, stock->quantity, stock->averagePrice);
    return 1;
}

static void buy_screen(Portfolio *p)
{
    char symbol[SYMBOL_SIZE];
    int quantity;
    double price;
    puts("\n--- Buy Stock ---");
    if (read_symbol(symbol) && read_integer("Quantity: ", 1, INT_MAX, &quantity)
        && read_price(&price))
        buy_stock(p, symbol, quantity, price);
}

/* 매도 직전 평균가로 손익을 계산하고, 검증이 끝나면 거래와 잔고를 함께 반영한다. */
static void sell_screen(Portfolio *p)
{
    char symbol[SYMBOL_SIZE];
    int quantity;
    double price;
    puts("\n--- Sell Stock ---");
    if (!read_symbol(symbol))
        return;
    int index = find_stock(p, symbol);
    if (index < 0 || p->stocks[index].quantity == 0) {
        puts("No shares held for this symbol.");
        return;
    }
    if (p->trade_count >= MAX_TRADES) {
        puts("Trade history is full.");
        return;
    }
    Stock *stock = &p->stocks[index];
    printf("Shares available: %d\n", stock->quantity);
    if (!read_integer("Quantity to sell: ", 1, stock->quantity, &quantity)
        || !read_price(&price))
        return;

    Portfolio before = *p;
    double profit = (price - stock->averagePrice) * quantity;
    double cumulative = stock->realizedProfit + profit;
    if (!isfinite(profit) || !isfinite(cumulative)) {
        puts("Realized profit exceeds the supported range. Sale cancelled.");
        return;
    }

    Trade *trade = &p->trades[p->trade_count++];
    memcpy(trade->symbol, symbol, strlen(symbol) + 1);
    trade->type = TRADE_SELL;
    trade->quantity = quantity;
    trade->price = price;
    trade->realized_profit = profit;
    stock->realizedProfit = cumulative;
    stock->quantity -= quantity;
    if (stock->quantity == 0) {
        stock->averagePrice = 0.0;
        stock->currentPrice = 0.0;
    }
    if (!save_portfolio(p)) { *p = before; puts("Save failed. Sale cancelled."); return; }
    printf("Sale complete: %s | Held: %d | Average: %.2f USD | "
           "Realized: %.2f USD | Cumulative: %.2f USD\n",
           symbol, stock->quantity, stock->averagePrice, profit, cumulative);
}

/* 현재가 미입력, 계산 범위 초과, 원가 0인 수익률은 N/A로 표시한다. */
static void print_metric(const char *label, double value, const char *unit)
{
    printf(" | %s: ", label);
    if (isfinite(value))
        printf("%.2f%s", value, unit);
    else
        printf("N/A");
}

static double return_percent(double profit, double cost)
{
    if (!isfinite(cost) || cost <= 0.0)
        return NAN;
    return profit / cost * 100.0;
}

typedef struct {
    const Stock *stock;
    double value, profit, rate;
} HoldingAnalysis;

typedef struct {
    HoldingAnalysis rows[MAX_STOCKS];
    int count;
    double cost, value, profit, realized;
} Analysis;

/* 조회와 심화 분석에서 같은 보유 원가, 평가액, 손익을 사용한다. */
static Analysis analyze(const Portfolio *p)
{
    Analysis a = {0};
    for (int i = 0; i < p->stock_count; ++i) {
        const Stock *s = &p->stocks[i];
        a.realized += s->realizedProfit;
        if (!s->quantity)
            continue;
        HoldingAnalysis *row = &a.rows[a.count++];
        double price = s->currentPrice > 0 ? s->currentPrice : NAN;
        double cost = s->averagePrice * s->quantity;
        row->stock = s;
        row->value = price * s->quantity;
        row->profit = (price - s->averagePrice) * s->quantity;
        row->rate = return_percent(row->profit, cost);
        a.cost += cost;
        a.value += row->value;
        a.profit += row->profit;
    }
    return a;
}

/* 보유량과 현재가를 종목별로 보여 준다. */
static void print_portfolio(const Portfolio *p)
{
    Analysis a = analyze(p);
    puts("\n--- Portfolio (USD) ---");
    for (int i = 0; i < a.count; ++i) {
        HoldingAnalysis row = a.rows[i];
        const Stock *s = row.stock;
        printf("%s | Qty: %d | Average: %.2f", s->symbol, s->quantity, s->averagePrice);
        print_metric("Current", s->currentPrice > 0 ? s->currentPrice : NAN, "");
        print_metric("Value", row.value, "");
        print_metric("Unrealized", row.profit, "");
        print_metric("Return", row.rate, "%");
        print_metric("Realized", s->realizedProfit, "");
        putchar('\n');
    }
    if (!a.count) puts("No shares held.");
    printf("TOTAL");
    print_metric("Value", a.value, "");
    print_metric("Unrealized", a.profit, "");
    print_metric("Return", return_percent(a.profit, a.cost), "%");
    print_metric("Realized", a.realized, "");
    putchar('\n');
}

/* 삽입 정렬: 앞쪽의 낮은 수익률을 한 칸씩 밀어 현재 행을 넣는다. */
static void sort_returns(Analysis *a)
{
    for (int i = 1; i < a->count; ++i) {
        HoldingAnalysis row = a->rows[i];
        if (!isfinite(row.rate)) continue;
        int j = i;
        while (j > 0 && (!isfinite(a->rows[j-1].rate) || row.rate > a->rows[j-1].rate)) {
            a->rows[j] = a->rows[j-1];
            --j;
        }
        a->rows[j] = row;
    }
}

/* 최고 또는 최저 수익률에 해당하는 동률 종목을 모두 출력한다. */
static void print_extreme(const Analysis *a, const char *label, double rate)
{
    printf("%s:", label);
    if (isfinite(rate)) {
        for (int i = 0; i < a->count; ++i)
            if (a->rows[i].rate == rate)
                printf(" %s", a->rows[i].stock->symbol);
        printf(" (%.2f%%)\n", rate);
    } else puts(" N/A");
}

/* 전체 평가액을 분모로 비중을 구하고 정렬된 유효 구간의 양 끝을 읽는다. */
static void print_advanced(const Portfolio *p)
{
    Analysis a = analyze(p);
    sort_returns(&a);
    puts("\n--- Advanced Analysis (USD) ---");
    printf("TOTAL");
    print_metric("Value", a.value, "");
    print_metric("Realized", a.realized, "");
    print_metric("Unrealized", a.profit, "");
    putchar('\n');
    if (!a.count) puts("No shares held.");
    int ranked = 0;
    for (int i = 0; i < a.count; ++i) {
        HoldingAnalysis row = a.rows[i];
        printf("%s", row.stock->symbol);
        print_metric("Value", row.value, "");
        double weight = isfinite(a.value) && a.value > 0 ? row.value / a.value * 100 : NAN;
        print_metric("Weight", weight, "%");
        print_metric("Return", row.rate, "%");
        putchar('\n');
        if (isfinite(row.rate)) ++ranked;
    }
    print_extreme(&a, "Highest rated return", ranked ? a.rows[0].rate : NAN);
    print_extreme(&a, "Lowest rated return", ranked ? a.rows[ranked-1].rate : NAN);
    if (ranked < a.count)
        puts("Ranking covers holdings with a known return. Update missing prices in menu 3.");
}

/* 현재가를 하나 갱신할 때마다 같은 화면에서 전체 평가 결과를 다시 보여 준다. */
static void portfolio_screen(Portfolio *p)
{
    for (;;) {
        int choice;
        char symbol[SYMBOL_SIZE];
        print_portfolio(p);
        if (!read_integer("1. Update current price / 0. Main menu: ", 0, 1, &choice) || !choice)
            return;
        if (!read_symbol(symbol))
            return;
        int index = find_stock(p, symbol);
        if (index < 0 || !p->stocks[index].quantity) {
            puts("No shares held for this symbol.");
            continue;
        }
        double previous = p->stocks[index].currentPrice;
        if (!read_price(&p->stocks[index].currentPrice))
            return;
        if (!save_portfolio(p)) {
            p->stocks[index].currentPrice = previous;
            puts("Save failed. Price update cancelled.");
        }
    }
}

static void print_menu(void)
{
    printf("============================\n");
    printf(" STOCK PORTFOLIO MANAGER\n");
    printf("============================\n");
    printf("1. Buy Stock\n");
    printf("2. Sell Stock\n");
    printf("3. View Portfolio\n");
    puts("4. Advanced Analysis");
    printf("0. Exit\n");
}

int main(void)
{
    char *cwd = _getcwd(NULL, 0);
    if (cwd) { printf("Data directory: %s\n", cwd); free(cwd); }
    HANDLE lock = CreateFileW(L"portfolio.lock", GENERIC_WRITE, 0, NULL,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (lock == INVALID_HANDLE_VALUE) {
        puts("Cannot open data directory for writing or another instance is running.");
        return 1;
    }
    Portfolio portfolio = {0};
    if (!load_portfolio(&portfolio)) {
        puts("Load failed: missing, damaged or invalid portfolio files. Data preserved.");
        CloseHandle(lock);
        return 1;
    }
    int choice;
    for (;;) {
        print_menu();
        if (!read_integer("Select: ", 0, 4, &choice) || choice == 0)
            break;
        if (choice == 1)
            buy_screen(&portfolio);
        else if (choice == 2)
            sell_screen(&portfolio);
        else if (choice == 3)
            portfolio_screen(&portfolio);
        else
            print_advanced(&portfolio);
    }
    CloseHandle(lock);
    return 0;
}
