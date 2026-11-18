# Stock Portfolio Manager

대학교 C Programming 기말 프로젝트. C17과 Windows 콘솔을 사용하여 미국 주식의 보유 현황과 거래 기록을 관리한다. 개발 규칙은 `AGENTS.md`를 따른다.

## 현재 상태와 프로젝트 구조

- `AGENTS.md`: 코딩 규칙, 금융 데이터 처리 규칙, 작업 및 제출 절차.
- `StockPortfolio/StockPortfolio.slnx`: 실제 Visual Studio 솔루션. 아래 프로젝트 하나를 연결한다.
- `StockPortfolio/StockPortfolio/StockPortfolio.vcxproj`: MSVC v145 콘솔 프로젝트. Debug/Release, Win32/x64 구성과 C17 설정을 사용한다.
- `StockPortfolio/StockPortfolio/main.c`: 유일한 C 진입점. 메뉴 1에서 매수하고 보유 수량과 평균 매입가를 확인한다. 메뉴 0 또는 EOF에서 종료한다. 메뉴 2에서는 보유 종목을 매도하고 거래 손익과 종목별 누적 실현손익을 확인한다. 메뉴 3에서 보유 종목과 손익을 조회한다.
- `StockPortfolio.vcxproj.filters`: 솔루션 탐색기의 소스·헤더·리소스 분류. 실제 폴더와는 별개다.
- `StockPortfolio.vcxproj.user`: 사용자별 설정. 현재 별도 항목이 없다.
- `StockPortfolio/x64/Debug/`: 기존 실행 파일과 디버깅 정보.
- `StockPortfolio/StockPortfolio/x64/Debug/`: 컴파일 중간 파일과 빌드 기록. 과거 FileName 관련 산출물도 남아 있다.
- 루트 및 솔루션의 `.vs/`: Visual Studio 상태, 검색 인덱스, 데이터베이스, Copilot 세션 등의 자동 생성 자료.
- `.vs/stockportfolio/`: 별도 C++ Hello World 프로젝트. 현재 솔루션에 연결되지 않아 현재 빌드 대상이 아니다. 해당 폴더는 삭제하지 않았다.

전체 파일 목록을 확인하고 규칙·소스·주 프로젝트 설정을 읽었다. 바이너리 캐시와 채팅 기록은 프로그램 소스로 취급하지 않는다.

## 권장 폴더 구성 (향후 제안)

기존 솔루션 위치와 `main.c`를 유지한다. 필요한 기능을 구현할 때 해당 폴더와 파일을 추가한다. 이번에는 아래 폴더를 생성하지 않았다.

- `StockPortfolio/StockPortfolio/src/`: input.c, portfolio.c, storage.c 등 기능별 구현.
- `StockPortfolio/StockPortfolio/include/`: portfolio.h, input.h, storage.h 등 구조체와 함수 선언.
- `StockPortfolio/StockPortfolio/data/`: 실행 중 저장할 데이터. 상대 경로의 기준은 실행 작업 디렉터리이므로 저장 기능 구현 시 경로를 명시한다.
- `tests/`: 계산 검증 및 입력 오류 테스트. 테스트 진입점은 앱 프로젝트에 함께 등록하지 않는다.
- `docs/`: 실행 화면, 테스트 기록, 발표자료, AI 활용 기록.

새 C 파일은 프로젝트의 ClCompile 항목에, 헤더는 ClInclude 항목에 등록하고 filters도 함께 갱신한다. include 폴더를 추가할 때는 포함 디렉터리 설정도 연결한다. main.c를 중복 생성하지 않는다.

## 데이터 구조체와 거래 규칙

초기 버전은 정수 주식 수량과 고정 크기 배열을 사용한다. 금액과 가격은 USD이며 double로 계산하고 출력할 때 소수점 둘째 자리까지 표시한다. 계산 중간에 반올림하지 않는다.

```c
#define MAX_STOCKS 100
#define MAX_TRADES 1000
#define SYMBOL_SIZE 16

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
    double realized_profit; /* 매수는 0, 매도는 해당 거래 손익 */
} Trade;

typedef struct {
    Stock stocks[MAX_STOCKS];
    int stock_count;
    Trade trades[MAX_TRADES];
    int trade_count;
} Portfolio;
```

- 매수 후 평균가 = (기존 수량 × 기존 평균가 + 매수 수량 × 매수가) / 새 수량.
- 매도 실현손익 = (매도가 − 매도 직전 평균가) × 매도 수량. 일부 매도 후 평균가는 유지한다.
- 전량 매도 시 수량과 평균가를 0으로 설정하고 종목의 누적 실현손익과 거래 이력은 유지한다. 재매수하면 새 매수가부터 평균가를 계산한다.
- 평가손익 = (현재가 − 평균가) × 보유 수량. currentPrice는 0으로 초기화하고 양수인 현재가를 입력받는다. 보유 중인 종목의 현재가가 0이면 평가 불가로 표시하며 합계도 불완전함을 알린다.
- Stock.realizedProfit은 0으로 초기화하고 매도할 때 해당 거래의 Trade.realized_profit을 한 번 더한다. 전체 실현손익은 모든 종목의 Stock.realizedProfit 합계이며, 같은 거래를 이중 합산하지 않는다. 평가손익과 별도로 표시한다.
- 거래 수량은 양수, 가격은 유한한 양수여야 한다. 초과 매도, 숫자 범위 초과, 배열 용량 초과를 거부한다. 실패 시 보유 상태와 거래 이력을 모두 유지한다.
- 종목 코드는 공백 없이 대문자로 정규화하고 길이를 검증한다. 초기 버전은 영문·숫자·점·하이픈을 허용한다.
- 수수료와 세금은 초기 버전에서 제외한다. 자동 시세 조회와 실제 증권 주문은 실행하지 않는다.

## 기능별 함수 목록 (설계안)

- 메뉴: `void print_menu(void)`, `int read_menu_choice(int *choice)` — 메뉴 표시와 선택 검증.
- 입력: `int read_positive_int(int *value)`, `int read_positive_double(double *value)`, `int read_symbol(char *buffer, int capacity)` — fgets 기반 입력, 초과 길이·잔여 문자·EOF 처리.
- 초기화: `void init_portfolio(Portfolio *portfolio)`.
- 검색: `int find_stock(const Portfolio *portfolio, const char *symbol)` — 인덱스 또는 -1 반환.
- 매수: `int buy_stock(Portfolio *portfolio, const char *symbol, int quantity, double price)` — 종목 추가, 평균가 계산, 거래 기록.
- 매도: `static void sell_screen(Portfolio *p)` — 보유량 확인 및 실현손익 기록.
- 현재가: `int update_price(Portfolio *portfolio, const char *symbol, double price)`.
- 평가: `int calculate_unrealized_profit(const Stock *stock, double *profit)` — 현재가가 없으면 실패 반환.
- 실현손익: `double calculate_realized_profit(const Portfolio *portfolio)`.
- 출력: `void print_portfolio(const Portfolio *portfolio)`, `void print_trade_history(const Portfolio *portfolio)`.
- 저장: `int save_portfolio(const Portfolio *portfolio, const char *path)`.
- 불러오기: `int load_portfolio(Portfolio *portfolio, const char *path)` — 임시 구조체에 읽고 전체 검증 후 반영.

int 상태 반환은 원칙적으로 성공 1, 실패 0으로 통일한다(검색 함수 제외). 저장 형식은 버전과 보유·거래 개수를 포함하는 텍스트 형식을 계획한다. 구조체 메모리를 그대로 저장하지 않는다. 손상 파일과 개수 초과를 검증하고, 저장 실패 시 기존 파일을 보존하는 방식을 구현한다.

## 단계별 개발 계획

1. 초기 단계: 구조 분석, main.c 연결 복구, C17 설정, README 작성 및 기존 메뉴 빌드 확인.
2. 메뉴 선택·반복·종료 구현. 잘못된 문자, 범위 밖 숫자, EOF를 검증한다.
3. 구조체와 초기화·검색 구현. 빈 목록과 중복 종목 검색을 검증한다.
4. 매수 기능 구현. 신규 종목과 추가 매수의 이동평균, 용량 초과를 검증한다.
5. 매도 기능 구현. 일부·전량·초과 매도와 실현손익을 검증한다.
6. 수동 현재가와 평가 조회 구현. 미입력 가격과 합계 표시를 검증한다.
7. 거래 이력 조회 구현. 매수·매도 기록과 손익 합계를 검증한다.
8. 파일 저장·불러오기 구현. 재실행 복원과 누락·손상 파일을 검증한다.
9. 제출 정리: 소스, README, 실행 화면, 테스트 기록, 발표자료, AI 활용 내용을 준비한다.

각 단계에서 하나의 기능 단위씩 구현하고 중요한 함수에 한국어 설명을 작성한다. 변경 이유와 실제 수행한 검사 결과를 기록한다.

## 빌드 및 실행 방법

1. Visual Studio에서 `StockPortfolio/StockPortfolio.slnx`를 연다.
2. Debug 및 x64를 선택한다. MSVC v145와 Windows SDK가 필요하다.
3. Ctrl+Shift+B로 솔루션을 빌드한다.
4. Ctrl+F5로 실행한다. 메뉴 1에서 종목 코드, 수량, 주당 가격(USD)을 입력한다. 완료 메시지에서 종목별 보유 수량과 평균가를 확인한 뒤 메뉴 0으로 종료한다. 보유 정보와 매수 기록은 실행 중 메모리에 저장한다.

## 테스트 계획

2026-11-18 검증: MSVC v145로 솔루션의 Debug/x64 구성을 임시 출력 경로에서 빌드하여 성공했다. 생성된 실행 파일에서 제목과 메뉴 출력, 종료 코드 0을 확인했다. 임시 출력 경로 때문에 MSB8029 경고가 발생했으며 컴파일·링크 오류는 없었다. Release와 Win32 구성은 실행 검증하지 않았다.

향후 계산 예: 10주를 100 USD에 매수하고 10주를 120 USD에 추가 매수하면 평균가는 110 USD이다. 5주를 130 USD에 매도하면 실현손익은 100 USD, 남은 수량은 15주이다. 현재가가 120 USD이면 평가손익은 150 USD이다. 이 계산 테스트는 아직 실행하지 않았다.

## 이번 변경 기록

- README.md 신규 작성: 프로젝트 분석, 설계, 개발·검증·제출 계획 문서화.
- StockPortfolio.vcxproj: 존재하지 않는 FileName.c 대신 기존 main.c를 연결하고 네 빌드 구성에 C17을 지정.
- StockPortfolio.vcxproj.filters: 소스 파일 표시를 main.c에 맞춤.
- 기존 main.c와 솔루션은 유지. 거래 기능과 구조체는 아직 구현하지 않음.

- 2026-11-18: 기존 솔루션을 Debug/x64로 다시 빌드했다. 기본 출력 경로인 StockPortfolio/x64/Debug/StockPortfolio.exe를 실행해 매수·매도·조회·종료 메뉴 출력과 종료 코드 0을 확인했다.

## 매수 기능 구현 및 검증

기존 main.c에 메뉴 반복, 입력 검증, 종목 검색, 매수 처리를 함수로 나누어 구현했다. 최대 100종목과 매수 기록 1,000건을 저장한다. 종목 수가 한도에 도달해도 기존 종목은 추가 매수할 수 있다. 거래 기록이 가득 차면 매수를 거부한다.

Visual Studio에서 Debug/x64로 실행하고 아래 순서로 확인한다.

1. 메뉴 1에서 aapl, 10, 100을 입력한다. AAPL 10주, 평균 100.00 USD가 표시된다.
2. 같은 실행에서 메뉴 1을 다시 선택하고 AAPL, 5, 130을 입력한다. AAPL 15주, 평균 110.00 USD가 표시된다.
3. 새 매수에서 수량 0이나 가격 -1을 입력한다. 오류 안내 후 해당 항목을 다시 입력할 수 있다.
4. 메뉴 0을 선택해 종료한다.

2026-11-18에 실제 Debug/x64 실행 파일로 가중평균, 대소문자 통합, 잘못된 입력의 재입력, 종목·거래 용량 한도, 보유 수량 오버플로, 입력 중 EOF를 검증했다.

## 매도 기능과 확인 방법

기존 main.c의 메뉴 2에서 종목, 매도 수량, 주당 매도가(USD)를 입력한다. 보유량을 초과한 수량은 다시 입력받는다. 전량 매도하면 수량과 평균가는 0이 되며 종목별 누적 실현손익은 재매수 후에도 이어진다.

Debug/x64 실행 검증: 같은 실행에서 AAPL을 100 USD에 10주, 130 USD에 5주 매수한다. 130 USD에 5주 매도하면 10주가 남고 평균가는 110 USD, 거래 및 누적 실현손익은 +100 USD이다. 남은 10주를 100 USD에 매도하면 거래 손익은 -100 USD이고 누적 손익은 0 USD이다. 미보유 종목, 초과 수량, 잘못된 가격, 손익 범위 초과, 입력 중단도 검증했다.

## 조회와 손익 분석

메뉴 3에서 보유 수량, 평균 매입가, 현재가, 평가액, 미실현손익, 수익률, 누적 실현손익을 조회한다. 조회 화면에서 1을 선택해 종목 코드와 양수인 현재가(USD)를 입력하면 즉시 다시 계산한다. 0을 선택하면 메인 메뉴로 돌아간다. 입력한 현재가는 같은 실행 중에 유지되며 전량 매도 시 초기화된다.

전체 수익률은 전체 미실현손익 / 전체 보유 매입원가 × 100이다. 현재가 미입력, 표현 범위 초과 또는 수익률의 분모가 0인 경우 해당 항목은 N/A다. 전량 매도한 종목도 전체 실현손익에는 포함한다.

Debug/x64 실행 검증: A를 100 USD와 120 USD에 각 10주 매수하고 130 USD에 5주 매도한다. B를 20 USD에 5주 매수한다. 현재가를 A 120 USD, B 10 USD로 입력하면 전체 평가액 1,850 USD, 미실현손익 +100 USD, 수익률 5.71%, 누적 실현손익 +100 USD이다. 이후 전량 매도와 빈 포트폴리오의 수익률 N/A도 확인했다.

## CSV 저장 위치와 복원

프로그램은 시작할 때 Data directory로 실제 작업 디렉터리를 출력한다. 이 디렉터리에 CSV와 선택 파일을 저장한다. 현재 Visual Studio 프로젝트에는 디버깅 작업 디렉터리 재정의가 없으므로 기본값은 프로젝트 파일이 있는 다음 폴더다.

    C:\Users\신영진\OneDrive - 경운대학교\문서\projects\Stock-Portfolio-Manger\StockPortfolio\StockPortfolio

터미널에서 exe의 전체 경로를 실행하면 터미널의 현재 폴더가 저장 위치다. 같은 데이터를 다시 열려면 같은 작업 디렉터리에서 실행한다.

매수·매도·현재가 수정이 성공할 때마다 전체 보유 정보를 holdings-0.csv 또는 holdings-1.csv에, 거래 이력을 같은 번호의 trades CSV에 기록한다. portfolio.index의 두 번째 필드가 현재 유효한 번호다. 두 CSV 작성을 마친 뒤 선택 파일을 교체한다. 이전 번호의 파일이 남는 것은 정상이다. 거래 이력은 그대로 읽고 보유 수량에 다시 적용하지 않는다. portfolio.lock은 같은 폴더에서 두 프로그램이 동시에 쓰는 것을 막는다.

저장 파일이 전혀 없으면 빈 포트폴리오로 시작한다. 기존 저장본의 일부가 없거나 CSV의 열, 값, 행 수, 파일 지문이 잘못되면 오류를 알리고 종료한다. 기존 데이터를 옮기거나 백업할 때는 프로그램을 종료하고 portfolio.index와 그 번호의 CSV 두 개를 함께 복사한다. 저장 중 오류가 발생하면 해당 매수·매도·가격 변경을 취소한다.

### 직접 확인하는 절차

1. 기존 보유 데이터와 분리하려면 새 빈 폴더에서 터미널을 열고 빌드된 StockPortfolio.exe를 전체 경로로 실행한다. Data directory가 새 폴더인지 확인한다.
2. 메뉴 1로 AAPL 10주를 100 USD에 매수하고 메뉴 2로 3주를 120 USD에 매도한다. 메뉴 3에서 현재가를 110 USD로 입력한 뒤 메인 메뉴에서 0으로 종료한다.
3. 같은 폴더에서 다시 실행하고 메뉴 3을 연다. AAPL 7주, 평균 100 USD, 평가액 770 USD, 미실현손익 70 USD, 실현손익 60 USD가 표시된다.
4. 거래 없이 한 번 더 재시작한다. 현재 슬롯의 trades CSV는 헤더와 매수·매도 두 줄로 유지된다.
5. 손상 검사는 복사본 폴더에서만 진행한다. 활성 거래 CSV의 끝에 잘못된 행을 추가하고 실행하면 Load failed가 표시된다.

Debug/x64 실행 파일로 위 복원 값과 거래 행 수, 빈 폴더 시작, 손상 CSV 거부를 확인했다. 별도 테스트 폴더에서 선택 파일 교체를 실패시켜 매수가 취소되고 재실행 시 이전 수량이 복원되는 것도 확인했다.

## 메뉴 4 · 심화 분석과 발표 설명

메뉴 3에서 현재가를 입력한 뒤 메인 메뉴 4를 선택한다. 금액은 USD이며 보유 주식만 자산 비중에 포함한다.

- analyze는 Portfolio의 Stock 배열을 한 번 순회한다. 종목별 평가액은 현재가 × 수량, 미실현손익은 (현재가 − 평균 매입가) × 수량이다. 보유 수량이 0이어도 과거 실현손익은 합산한다. 종목별 계산 결과는 별도 Analysis 배열에 담아 메뉴 3과 4가 함께 사용한다.
- return_percent는 미실현손익을 보유 매입원가로 나누고 100을 곱한다. 분모가 0이거나 표현할 수 없으면 수익률은 N/A다.
- sort_returns는 삽입 정렬로 높은 수익률부터 배치한다. 한 행씩 꺼내 앞쪽의 더 낮은 행을 뒤로 밀고 빈자리에 넣는다. 최대 100종목이며 최악 시간복잡도는 O(n²)이다. 동률은 등록 순서를 유지하고 계산할 수 없는 종목은 마지막에 둔다. 정렬 대상은 분석 배열이므로 보유 목록과 CSV 순서는 유지된다.
- print_extreme은 정렬된 유효 구간의 첫 수익률 또는 마지막 수익률과 같은 종목을 모두 출력한다. 비교는 반올림 전 수익률 기준이다.
- print_advanced는 전체 평가액, 실현손익, 미실현손익을 각각 표시하고, 종목 평가액 / 전체 평가액 × 100으로 비중을 계산한다. 현재가가 빠져 전체 평가액이 미정이면 모든 비중은 N/A이고 순위는 수익률을 계산할 수 있는 종목으로 제한한다.
- print_metric은 금액·백분율을 소수 둘째 자리까지 출력하고 계산할 수 없는 값은 N/A로 표시한다.

실행 예: B 2주를 50 USD에, A 10주를 100 USD에 매수하고 A 2주를 130 USD에 매도한다. A 현재가 110, B 현재가 40을 입력하면 A가 10.00%로 최고, B가 -20.00%로 최저다. 전체 평가액은 960.00 USD, 비중은 A 91.67%, B 8.33%, 실현손익과 미실현손익은 각각 +60.00 USD다. Debug/x64에서 이 예시와 빈 목록, 누락 가격, 동률, 전량 매도, CSV 복원 후 분석을 검증했다.

## 2026-11-18 C 코드 검토 및 회귀 테스트

검토 대상은 C 프로젝트의 전체 소스 main.c이다. C17 설정으로 Debug/Release × Win32/x64 네 구성을 다시 빌드하고 각 실행 파일에서 빈 포트폴리오 분석과 종료를 확인했다. 아래 상세 테스트는 기본 Debug/x64 실행 파일과 독립된 임시 작업 폴더를 사용했다.

- 수정 전: 첫 매수에서 portfolio.index.tmp 쓰기를 실패시키면 거래가 취소돼도 CSV가 남았다. 재시작하면 인덱스 없는 CSV를 손상으로 판단해 종료 코드 1로 끝났다.
- 수정 후: 첫 CSV 쓰기 전에 빈 상태를 EMPTY1 인덱스로 확정한다. 인덱스를 만들지 못하면 CSV 쓰기 전에 취소한다. 첫 CSV 쌍이 불완전해도 이미 확정된 빈 상태에서 재시작한다. 첫 인덱스 및 거래 CSV 쓰기 실패, 재시작, 장애 제거 후 매수를 확인했다. 기존 SP1 저장 형식과 두 슬롯은 계속 사용한다.
- 배열·버퍼: 종목 코드 15자 허용, 16자와 128자 입력 거부 후 정상 입력 처리. 100종목 이후 신규 종목 거부, 보유 수량 INT_MAX 초과 거부, 거래 1,000건 이후 추가 매수 거부와 1,000건 복원 확인.
- 입력: 잘못된 메뉴, 숫자 뒤 문자, 음수·0 수량, NaN·무한대·음수·0·범위 초과 가격 재입력 확인. 미보유 매도 거부, 초과 매도 수량 재입력 확인.
- 계산: 10주 × 100 USD 및 5주 × 130 USD 매수 → 평균 110 USD. 5주를 120 USD에 매도하고 현재가를 90 USD로 입력 → 잔량 10주, 평가액 900 USD, 실현 +50 USD, 미실현 −200 USD, 수익률 −18.18%. 빈 보유 목록의 수익률 N/A 및 심화 분석 진입 확인.
- CSV: 저장 후 재실행, 기존 매도 저장 실패 시 잔고 복구, 활성 CSV 손상 시 종료 코드 1 확인.
- 장시간 연속 입력 시험에서 예상 1,000건 대신 998건이 저장된 한 차례의 원인은 확인하지 못했다. 그 파일에서 재시작해 2건을 추가하고 다음 거래가 거부되는 것과 재시작 시 1,000건이 유지되는 것을 확인했다.
- 미실행: 메모리 검사기, 물리적 디스크 장애·전원 차단, 모든 입력 조합의 전수 테스트. Debug/x64 이외 구성은 빌드 및 기본 실행만 검사했다.
