# מפת ארכיטקטורה

מקום אחד שמרכז את כל מבנה הפרויקט.  
המערכת מדמה travelers הנעים על גרף מכוון ומשוקלל, ומתפתחת לאורך ה־milestones
מכלי שורת־פקודה שמריץ Dijkstra, אל סימולטור GUI מרובה־תהליכים עם IPC,
סנכרון כניסה לצמתים, ובחירה בין אלגוריתמי תזמון.

---

## 1. פריסת הפרויקט

```text
Linux_Project/
├── CMakeLists.txt        # הגדרות build עם CMake
├── Makefile              # פקודות milestone1..7
├── README.md             # הוראות build והרצה
├── ARCHITECTURE.md       # הקובץ הזה
│
├── Dijkstra.h            # טיפוסים והצהרות משותפות
├── Dijkstra.c            # גרף, Dijkstra, parsing helpers, path resolution
├── Dijkstra_main.c       # נקודת כניסה של milestone 1
├── main.c                # נקודת כניסה של הסימולטור (CLI args + scheduler)
├── GraphVisual.c         # GUI, travelers, fork, IPC, scheduling, animation
│
├── Graph.txt             # קובץ קלט ברירת מחדל
├── Graph_sjf_test.txt    # קובץ בדיקה להבדל בין FCFS ל-SJF
└── build/                # תיקיית build שנוצרת ע"י CMake
```

בשונה מפרויקטים שמחולקים ל־`src/` ו־`include/`, כאן רוב הלוגיקה נמצאת במספר קבצים
מרכזיים ברמת השורש.  
לכן כשמחפשים איפה לשנות משהו, לרוב מתחילים ב־`Dijkstra.c`, `main.c`,
או `GraphVisual.c`.

---

## 2. Milestone → בינארי → נקודת כניסה → פקודת הרצה

| Milestone | בינארי | נקודת כניסה | פקודת הרצה |
| --- | --- | --- | --- |
| 1 | `dijkstra` | `Dijkstra_main.c` | `./dijkstra <file>` |
| 2 | `sim` | `main.c` → `runGraphVisualizer(...)` | `./sim <file>` |
| 3 | `sim` | `main.c` → `runGraphVisualizer(...)` | `./sim <file>` |
| 4 | `sim4` | `main.c` → `runGraphVisualizer(...)` | `./sim4 <file>` |
| 5 | `sim` | `main.c` → `runGraphVisualizer(...)` | `./sim <file>` |
| 6 | `sim` | `main.c` → `runGraphVisualizer(...)` | `./sim <file>` |
| 7 | `sim` | `main.c` → `runGraphVisualizer(filename, scheduler)` | `./sim -schd fcfs|sjf <file>` |

### הערה חשובה
במבנה הנוכחי של הפרויקט:
- `milestone1` בונה את `./dijkstra`
- `milestone4` בונה את `./sim4`
- milestones 2, 3, 5, 6, 7 רצים דרך `./sim`

כלומר, היכולות של milestones 5–7 מצטברות מעל אותו entry flow, ולא דרך קבצי GUI נפרדים.

---

## 3. מה כל קובץ עושה

## `Dijkstra.h`
קובץ header מרכזי.

### אחריות
- טיפוסי הגרף (`Graph`, `Edge`)
- תוצאת shortest path (`DijkstraResult`)
- הצהרות לפונקציות משותפות
- API ציבורי בין `Dijkstra.c`, `main.c`, ו־`GraphVisual.c`

### נוגעים בו כש...
- מוסיפים פונקציה חדשה שצריכה להיות זמינה בין קבצים
- משנים struct משותף
- מוסיפים שדה חדש לתוצאת Dijkstra או לייצוג הגרף

---

## `Dijkstra.c`
המנוע האלגוריתמי של הפרויקט.

### אחריות
- טעינת גרף מהקלט
- יצירת adjacency list
- מימוש Dijkstra
- שחרור זיכרון
- איתור קובץ גרף (`resolveGraphPath(...)`)
- parsing helper functions

### כאן תחפש אם אתה רוצה לשנות:
- את shortest path algorithm
- את פורמט טעינת הגרף
- את הדרך שבה path מחושב או מוחזר
- validation של קלט

---

## `Dijkstra_main.c`
נקודת הכניסה של milestone 1.

### אחריות
- מצב שורת־פקודה בלבד
- טעינת הקלט
- הפעלת Dijkstra
- הדפסת path ומשקל

### כאן נוגעים רק אם:
- משנים את milestone 1
- רוצים לשנות את ההדפסה למסך ב־CLI

---

## `main.c`
נקודת הכניסה של מצב הסימולטור.

### אחריות
- קריאת ארגומנטים משורת הפקודה
- בחירת scheduler (`fcfs` / `sjf`)
- מציאת קובץ קלט
- קריאה ל־`runGraphVisualizer(...)`

### כאן תחפש אם אתה רוצה:
- להוסיף flag חדש
- לשנות syntax של ההרצה
- לשנות ברירת מחדל של scheduler
- להוסיף mode חדש

### דוגמה
אם תרצה בעתיד להוסיף:
- `-speed fast`
- `-debug`
- `-schd priority`

זה המקום הראשון לשנות.

---

## `GraphVisual.c`
הקובץ המרכזי של milestones 4–7.

### אחריות
- ציור הגרף עם raylib
- יצירה וניהול של traveler processes
- ניהול pipes ו־control pipes
- קבלת הודעות מה־children
- ניהול queue עבור כל node
- מימוש `FCFS` ו־`SJF`
- Play / Pause / Resume / Reset
- אנימציה של תנועה על קשתות
- הדפסת מצב לטרמינל

### בפועל
אם אתה משנה את ההתנהגות של הסימולטור — ברוב המקרים זה הקובץ שתיגע בו.

---

## `Graph.txt` / `Graph_sjf_test.txt`
קבצי קלט.

### אחריות
- תיאור הגרף
- תיאור travelers
- בדיקות והרצות דמו

### משתמשים בהם כדי:
- לבדוק correctness
- להדגים את difference בין FCFS ל־SJF
- לשחזר באגים

---

## `Makefile`
מעטפת build ידנית.

### אחריות
- `make milestone1`
- `make milestone4`
- `make milestone7`
- `make clean`

### כאן נוגעים אם:
- מוסיפים target חדש
- משנים שמות בינאריים
- משנים flags של קומפילציה

---

## `CMakeLists.txt`
מעטפת build עם CMake.

### אחריות
- build מסודר של הפרויקט
- linking מול raylib
- targets של הפרויקט

### כאן נוגעים אם:
- מוסיפים קובץ מקור חדש
- משנים מבנה build
- רוצים target נפרד נוסף

---

## `README.md`
מסמך ההרצה וההגשה.

### כאן נוגעים אם:
- milestone חדש מתווסף
- פקודות build משתנות
- פקודות run משתנות
- צריך להסביר scheduler חדש או test file חדש

---

## 4. זרימת המערכת בזמן ריצה

## מצב 1 — milestone 1
```text
Dijkstra_main.c
   → resolveGraphPath()
   → load graph
   → dijkstra()
   → print result
   → free memory
```

---

## מצב 2 — מצב GUI / סימולציה
```text
main.c
   → parse CLI args
   → detect scheduler
   → resolve graph file
   → runGraphVisualizer(filename, scheduler)
```

בתוך `GraphVisual.c`:

```text
runGraphVisualizer(...)
   → load graph
   → parse travelers
   → initialize visual graph
   → spawn child per traveler
   → parent GUI loop:
        - receive messages
        - schedule node entry
        - update animation state
        - draw graph/travelers/UI
```

---

## 5. Parent / Child model

## Parent process
האבא אחראי על:
- חלון raylib
- ציור nodes / edges / travelers
- קבלת הודעות מכל הילדים
- קבלת החלטה מי נכנס לצומת
- ניהול queue לכל node
- יישום `FCFS` או `SJF`
- Play / Pause / Resume / Reset

## Child process
כל ילד אחראי על traveler אחד:
- טוען את הגרף
- מחשב shortest path לעצמו
- שולח בקשה לכניסה לצומת
- מחכה לאישור מהאבא
- שולח update על entered / leaving / finished
- מתקדם לאורך המסלול שלו

---

## 6. החוזה בין הילד לאבא

התקשורת היא דו־כיוונית:

### Child → Parent
דרך `pipeFd`
- בקשה להיכנס לצומת
- entered node
- leaving node
- finished

### Parent → Child
דרך `controlPipe`
- אישור כניסה לצומת מסוים

### הזרימה הלוגית
```text
child sends REQUEST_NODE
parent enqueues traveler
parent selects next traveler by scheduler
parent sends approval
child sends ENTERED_NODE
child waits inside node
child sends LEAVING_NODE
parent frees node and dispatches next
child eventually sends FINISHED
```

---

## 7. מבני הנתונים הכי חשובים

## `Graph`
מייצג את הגרף.

### כולל בדרך כלל
- מספר צמתים
- רשימות שכנות
- קשתות ומשקלים

---

## `DijkstraResult`
תוצאת shortest path.

### כולל בדרך כלל
- האם נמצא מסלול
- המרחק הכולל
- המערך של הצמתים במסלול
- אורך המסלול

---

## `Traveler`
מצב traveler בצד האב.

### כולל
- `src`, `dest`
- `pid`
- `currentNode`, `nextNode`
- `finished`, `started`
- `moving`, `waitingForNode`, `insideNode`
- `pipeFd`, `controlPipe`
- שדות animation כמו:
  - `drawFromNode`
  - `drawToNode`
  - `moveStartTime`
  - `edgeDuration`

---

## `TravelerMessage`
ההודעה שהילד שולח לאבא.

### מייצגת
- איזה traveler שלח
- מה type ההודעה
- באיזה node הוא נמצא
- מה ה־next node
- מה משקל הקשת הבאה
- האם זו destination

---

## `WaitingTraveler`
רשומה בתור של node.

### משמשת ל
- שמירת traveler שמחכה להיכנס לצומת
- שמירת arrival order
- שמירת next edge weight לצורך `SJF`

---

## `NodeQueue`
התור של כל node.

### כולל
- מי כרגע בפנים (`occupiedBy`)
- מי מחכה
- `queueSize`

---

## 8. "אני רוצה לשנות X — איפה נוגעים?"

## לשנות את shortest path
לך ל:
- `Dijkstra.c`
- `Dijkstra.h`

---

## לשנות את פורמט הקלט
לך ל:
- `Dijkstra.c` אם זה parsing של הגרף
- `GraphVisual.c` אם זה parsing של travelers

---

## לשנות scheduler
לך ל:
- `main.c` — כדי להוסיף או לשנות CLI flag
- `GraphVisual.c` — כדי לממש את האלגוריתם

חפש:
- `SchedulerType`
- `pickNextTravelerIndex(...)`
- `tryDispatchNextTraveler(...)`
- `processTravelerMessages(...)`

---

## לשנות את חוקי הכניסה לצומת
לך ל:
- `GraphVisual.c`

חפש:
- `NODE_STAY_SECONDS`
- `NodeQueue`
- `WaitingTraveler`
- `MSG_REQUEST_NODE`
- `MSG_ENTERED_NODE`
- `MSG_LEAVING_NODE`
- `MSG_FINISHED`

---

## לשנות את האנימציה
לך ל:
- `GraphVisual.c`

חפש:
- `drawNode(...)`
- `drawEdge(...)`
- `travelerColor(...)`
- לולאת הציור בתוך `runGraphVisualizer(...)`

---

## לשנות Play / Pause / Reset
לך ל:
- `runGraphVisualizer(...)` בתוך `GraphVisual.c`

חפש:
- `IsMouseButtonPressed(...)`
- `CheckCollisionPointRec(...)`

---

## לשנות מה מודפס לטרמינל
לך ל:
- `processTravelerMessages(...)` בתוך `GraphVisual.c`

---

## לשנות build
לך ל:
- `Makefile`
- `CMakeLists.txt`

---

## לשנות תיעוד
לך ל:
- `README.md`
- `ARCHITECTURE.md`

---

## 9. Runtime flow לפי milestone

## Milestone 4
- נוסעים מרובים
- `fork()`
- GUI הורה + children
- עדיין בלי scheduler מלא של milestone 7

## Milestone 5
- IPC מסודר
- כל child מחשב shortest path בעצמו
- האב מצייר לפי הודעות

## Milestone 6
- סנכרון גישה לצמתים
- רק traveler אחד יכול להיות בתוך node בכל רגע

## Milestone 7
- בחירה בין `FCFS` ל־`SJF`
- queue לכל node
- החלטת dispatch בצד האב
- `SJF` מוגדר לפי משקל הקשת הבאה

---

## 10. איך לגשת לבאג

## אם traveler לא זז
בדוק:
- `MSG_LEAVING_NODE`
- `moveStartTime`
- `edgeDuration`
- חישוב progress בציור

## אם traveler לא נכנס לצומת
בדוק:
- `MSG_REQUEST_NODE`
- `enqueueTraveler(...)`
- `tryDispatchNextTraveler(...)`
- `sendEnterApproval(...)`
- `waitForEnterApproval(...)`

## אם `SJF` לא בוחר נכון
בדוק:
- `nextEdgeWeight`
- `arrivalOrder`
- `pickNextTravelerIndex(...)`
- מתי מכניסים traveler לתור

## אם Reset לא עובד
בדוק:
- `cleanupTravelerProcess(...)`
- `resetTravelerState(...)`
- `spawnTravelers(...)`
- איפוס `nodeQueues`

## אם build נכשל
בדוק:
- `Makefile`
- `CMakeLists.txt`
- raylib include / link flags

---

## 11. דרך עבודה מומלצת

כשאתה רוצה להוסיף פיצ'ר חדש:

1. תחליט אם הוא אלגוריתמי או ויזואלי
2. תמצא את הקובץ המרכזי:
   - אלגוריתם → `Dijkstra.c`
   - GUI / travelers / scheduler → `GraphVisual.c`
   - CLI → `main.c`
3. תבדוק אם צריך לעדכן header
4. תבנה ותבדוק עם test קטן
5. תבדוק גם `fcfs` וגם `sjf`
6. תעדכן README רק בסוף
7. לפני commit תריץ:
   - `git status`
   - `make clean`
   - `make milestone7`

---

## 12. פקודות שימושיות

### Build
```bash
make milestone1
make milestone4
make milestone7
make clean
```

### הרצה
```bash
./sim -schd fcfs Graph.txt
./sim -schd sjf Graph.txt
./sim -schd fcfs Graph_sjf_test.txt
./sim -schd sjf Graph_sjf_test.txt
```

### Git
```bash
git status
git tag
```

---

## 13. סיכום קצר

אם אתה שואל את עצמך:

- **מסלול קצר ביותר?** → `Dijkstra.c`
- **flags ו־CLI?** → `main.c`
- **GUI / travelers / pipes / scheduling / animation?** → `GraphVisual.c`
- **טיפוסים משותפים?** → `Dijkstra.h`
- **build?** → `Makefile` / `CMakeLists.txt`
- **תיעוד?** → `README.md` / `ARCHITECTURE.md`

זה המיפוי הכי חשוב לזכור.

---

## 14. הערה על מבנה המסמך

המסמך הזה נכתב בהשראת דף Architecture Map מסודר בסגנון “מפה אחת לכל הפרויקט”,
אבל הותאם למבנה הקיים של הפרויקט הזה — שהוא מבנה שטוח ופשוט יותר, בלי הפרדה ל־`src/` ו־`include/`.
