# דף עזר – פונקציות בפרויקט Graph Simulation

המסמך מחולק לשלושה חלקים:

1. פונקציות של `GraphVisual.c`
2. פונקציות של `Dijkstra.c`
3. פונקציות של `main.c`

בסוף המסמך מופיעים גם פורמט `Graph.txt`, סיכום מהיר ופקודות שימושיות למבחן.

---

# 1. פונקציות של `GraphVisual.c`

## מה המטרה של `GraphVisual.c`?

`GraphVisual.c` הוא הלב של הסימולציה הגרפית.

הקובץ אחראי על:

- פתיחת חלון באמצעות raylib.
- ציור הצמתים, הקשתות והמשקלים.
- ציור ה־travelers והאנימציה שלהם.
- יצירת תהליך בן לכל traveler באמצעות `fork()`.
- תקשורת בין האב לילדים באמצעות pipes.
- ניהול תור המתנה לכל צומת.
- בחירת traveler לפי FCFS או SJF.
- טיפול בכפתורי Play, Pause, Resume ו־Reset.
- ניקוי תהליכים וזיכרון בסיום.

הזרימה הכללית:

```text
main.c
→ מפעיל את runGraphVisualizer

runGraphVisualizer
→ קוראת את הגרף ואת ה-travelers
→ יוצרת תהליך בן לכל traveler
→ פותחת חלון GUI

כל תהליך בן:
→ מחכה ל-SIGUSR1
→ טוען את הגרף
→ מחשב Dijkstra לעצמו
→ מבקש להיכנס לצומת
→ מחכה לאישור מהאב
→ נכנס לצומת
→ נשאר בצומת
→ יוצא ומתקדם לקשת הבאה

תהליך האב:
→ מקבל הודעות מהילדים
→ מכניס בקשות לתורים
→ בוחר traveler לפי FCFS/SJF
→ שולח אישור לילד
→ מעדכן את ה-GUI
```

---

## `schedulerName`

```c
static const char *schedulerName(SchedulerType scheduler)
```

מקבלת את סוג המתזמן ומחזירה את השם שלו כמחרוזת.

```text
SCHED_FCFS → "FCFS"
SCHED_SJF  → "SJF"
```

משתמשים בפונקציה כדי להציג בחלון איזה scheduler פועל.

---

## `initNodeQueues`

```c
static void initNodeQueues(NodeQueue *nodeQueues, int numVertices)
```

מאפסת את תור ההמתנה של כל צומת.

לכל צומת היא מגדירה:

```text
occupiedBy = -1
queueSize = 0
collecting = 0
collectionStartTime = 0
```

המשמעות:

- `occupiedBy = -1` – הצומת פנוי.
- `queueSize = 0` – אין travelers בתור.
- `collecting = 0` – אין חלון איסוף פעיל.
- `collectionStartTime` – אין זמן התחלה שמור.

משתמשים בפונקציה בתחילת הסימולציה ולאחר Reset.

---

## `enqueueTraveler`

```c
static void enqueueTraveler(NodeQueue *nodeQueues,
                            const WaitingTraveler *wt)
```

מכניסה traveler לתור ההמתנה של הצומת שאליו הוא מבקש להיכנס.

לדוגמה, traveler שמבקש להיכנס לצומת 4 נכנס לתור:

```c
nodeQueues[4]
```

הפונקציה בודקת שהצומת חוקי ושהתור אינו מלא.

---

## `pickNextTravelerIndex`

```c
static int pickNextTravelerIndex(const NodeQueue *nodeQueues,
                                 int node,
                                 SchedulerType scheduler)
```

בוחרת מי יהיה ה־traveler הבא שייכנס לצומת.

### כאשר פועל FCFS

נבחר מי שהגיע ראשון לתור.

ההשוואה נעשית לפי:

```c
arrivalOrder
```

### כאשר פועל SJF

נבחר מי שמשקל הקשת הבאה שלו קטן יותר.

ההשוואה נעשית לפי:

```c
nextEdgeWeight
```

אם לשני travelers יש אותו משקל, הבחירה נעשית לפי סדר ההגעה.

זו הפונקציה המרכזית כאשר משימה קשורה לשינוי ההתנהגות של FCFS או SJF.

---

## `removeWaitingTraveler`

```c
static WaitingTraveler removeWaitingTraveler(NodeQueue *nodeQueues,
                                             int node,
                                             int idx)
```

מוציאה traveler מתור ההמתנה לאחר שהוא נבחר.

היא:

1. שומרת את ה־traveler שנמצא באינדקס `idx`.
2. מזיזה את שאר הממתינים מקום אחד קדימה.
3. מקטינה את `queueSize`.
4. מחזירה את ה־traveler שנבחר.

---

## `sendTravelerMessage`

```c
static int sendTravelerMessage(int fd,
                               const TravelerMessage *msg)
```

שולחת הודעה מתהליך בן לתהליך האב דרך pipe.

סוגי ההודעות:

```c
MSG_REQUEST_NODE
MSG_ENTERED_NODE
MSG_LEAVING_NODE
MSG_FINISHED
```

המשמעות:

- `MSG_REQUEST_NODE` – הילד מבקש להיכנס לצומת.
- `MSG_ENTERED_NODE` – הילד מודיע שנכנס לצומת.
- `MSG_LEAVING_NODE` – הילד מודיע שהוא יוצא מהצומת.
- `MSG_FINISHED` – הילד מודיע שסיים את המסלול.

---

## `sendEnterApproval`

```c
static int sendEnterApproval(int fd, int node)
```

תהליך האב משתמש בפונקציה כדי לשלוח לילד אישור להיכנס לצומת.

האישור נשלח דרך ה־control pipe וכולל את מספר הצומת שאושר.

---

## `waitForEnterApproval`

```c
static int waitForEnterApproval(int fd, int expectedNode)
```

תהליך הבן משתמש בפונקציה כדי לחכות לאישור מהאב.

הילד לא נכנס לצומת מיד לאחר הבקשה.

הוא נעצר בפונקציה הזו עד שהאב שולח אישור.

הפונקציה גם בודקת שמספר הצומת שהתקבל הוא הצומת שהילד ביקש.

---

## `startCollectionWindow`

```c
static void startCollectionWindow(NodeQueue *nodeQueues,
                                  int node,
                                  double now)
```

פותחת חלון איסוף קצר עבור צומת מסוים.

המטרה היא לא לבחור מיד את הראשון שהגיע, אלא להמתין זמן קצר כדי שגם בקשות שהגיעו כמעט באותו זמן ייכנסו לתור.

זה חשוב במיוחד ב־SJF, משום שהאב צריך לקבל כמה מועמדים לפני שהוא יכול להשוות את משקלי הקשתות שלהם.

---

## `tryDispatchNextTraveler`

```c
static void tryDispatchNextTraveler(NodeQueue *nodeQueues,
                                    Traveler *travelers,
                                    int numTravelers,
                                    int node,
                                    SchedulerType scheduler)
```

בוחרת ומאשרת את ה־traveler הבא שייכנס לצומת.

הפונקציה:

1. בודקת שהצומת פנוי.
2. בודקת שיש travelers שמחכים.
3. קוראת ל־`pickNextTravelerIndex`.
4. מוציאה את הנבחר מהתור.
5. מסמנת שהצומת תפוס על ידו.
6. שולחת לו אישור בעזרת `sendEnterApproval`.

במילים פשוטות, זו הפונקציה שבה האב אומר לילד:

```text
מותר לך להיכנס לצומת.
```

---

## `dispatchReadyNodes`

```c
static void dispatchReadyNodes(NodeQueue *nodeQueues,
                               int numVertices,
                               Traveler *travelers,
                               int numTravelers,
                               SchedulerType scheduler,
                               double now)
```

עוברת על כל הצמתים ובודקת האם הגיע הזמן לבחור traveler חדש.

עבור כל צומת היא בודקת:

- הצומת פנוי.
- יש travelers שמחכים.
- חלון האיסוף פעיל.
- עבר הזמן של `COLLECTION_WINDOW_SECONDS`.

כאשר כל התנאים מתקיימים, היא מפעילה את:

```c
tryDispatchNextTraveler(...)
```

---

## `sleepSeconds`

```c
static void sleepSeconds(double seconds)
```

מרדימה את התהליך למשך מספר שניות בעזרת `nanosleep`.

משתמשים בה עבור:

- זמן שהייה בתוך צומת.
- זמן תנועה על קשת לפי משקל הקשת.

---

## `resetTravelerState`

```c
static void resetTravelerState(Traveler *t)
```

מאפסת את מצב הריצה והאנימציה של traveler.

היא מאפסת בין היתר:

- PID.
- קצוות pipes.
- צומת נוכחי.
- צומת הבא.
- האם התחיל.
- האם סיים.
- האם הוא בתנועה.
- האם הוא ממתין.
- האם הוא נמצא בתוך צומת.
- זמני האנימציה.

משתמשים בה לפני יצירת הילדים ולאחר Reset.

---

## `cleanupTravelerProcess`

```c
static void cleanupTravelerProcess(Traveler *t)
```

מנקה תהליך של traveler.

הפונקציה:

1. שולחת `SIGTERM` אם הילד עדיין חי.
2. מחכה לו עם `waitpid`.
3. סוגרת את קצוות ה־pipes.
4. מאפסת את ה־PID.

משתמשים בה בעת Reset ובסגירת התוכנית.

---

## `reapFinishedTraveler`

```c
static void reapFinishedTraveler(Traveler *t)
```

מנקה תהליך בן שכבר סיים.

היא משתמשת ב־`waitpid` כדי שהתהליך לא יישאר בתור zombie process.

---

## `getEdgeEndpoints`

```c
static void getEdgeEndpoints(...)
```

מחשבת את נקודת ההתחלה ואת נקודת הסיום של קשת על המסך.

היא מתחשבת ברדיוס הצמתים, כך שהקו מתחיל בקצה העיגול ולא במרכזו.

---

## `computeLayout`

```c
static void computeLayout(VisGraph *vg, int W, int H)
```

מחשבת איפה כל צומת יוצג בחלון.

במימוש הנוכחי הצמתים מסודרים במעגל סביב מרכז החלון.

אם יש רק צומת אחד, הוא מוצג במרכז.

---

## `skipCommentsWS`

```c
static void skipCommentsWS(FILE *fp)
```

מדלגת על רווחים ועל שורות שמתחילות ב־`#`.

כך אפשר לכתוב הערות בתוך `Graph.txt` בלי שהן יפריעו לקריאת הנתונים.

---

## `loadVisGraph`

```c
static int loadVisGraph(const char *path, VisGraph *vg)
```

טוענת את חלק הגרף מהקובץ לתוך מבנה שמתאים לציור.

היא קוראת:

- מספר צמתים.
- מספר קשתות.
- מקור, יעד ומשקל של כל קשת.

היא אינה קוראת את חלק ה־travelers.

---

## `getEdgeWeight`

```c
static int getEdgeWeight(const VisGraph *vg, int from, int to)
```

מחפשת קשת בגרף הוויזואלי ומחזירה את המשקל שלה.

משתמשים במשקל כדי לחשב את משך האנימציה על הקשת.

---

## `getPathEdgeWeight`

```c
static int getPathEdgeWeight(Graph *g, int from, int to)
```

מחפשת את משקל הקשת ברשימת השכנים של הגרף האלגוריתמי.

תהליך הבן משתמש בה כדי לדעת מה משקל הקשת הבאה במסלול Dijkstra שלו.

---

## `drawArrowHead`

```c
static void drawArrowHead(...)
```

מציירת את ראש החץ של קשת מכוונת.

---

## `drawEdge`

```c
static void drawEdge(VisGraph *vg, int ei, Font font)
```

מציירת קשת אחת.

היא מציירת:

- קו.
- ראש חץ.
- משקל הקשת.

היא גם מטפלת בקשת עצמית, כלומר קשת מצומת אל עצמו.

---

## `drawNode`

```c
static void drawNode(VisGraph *vg,
                     int i,
                     Font font,
                     Traveler *travelers,
                     int numTravelers)
```

מציירת צומת אחד.

צבע הצומת משתנה לפי המצב:

- צומת מקור.
- צומת יעד.
- צומת שהוא גם מקור וגם יעד.
- צומת שנמצא בתוכו traveler.

בנוסף, מספר הצומת מוצג במרכז העיגול.

---

## `travelerColor`

```c
static Color travelerColor(int idx)
```

מחזירה צבע קבוע לכל traveler לפי האינדקס שלו.

המטרה היא שכל traveler יוצג בצבע שונה ב־GUI.

---

## `processTravelerMessages`

```c
static void processTravelerMessages(...)
```

זו אחת הפונקציות המרכזיות של תהליך האב.

היא קוראת את ההודעות שנשלחות מכל הילדים ומעדכנת את מצב הסימולציה.

### כאשר מתקבלת `MSG_REQUEST_NODE`

הפונקציה:

- מסמנת שה־traveler ממתין.
- יוצרת רשומת `WaitingTraveler`.
- שומרת את סדר ההגעה שלו.
- מכניסה אותו לתור.
- פותחת חלון איסוף אם הצומת פנוי.

### כאשר מתקבלת `MSG_ENTERED_NODE`

הפונקציה:

- מסמנת שה־traveler כבר אינו ממתין.
- מסמנת שהוא נמצא בתוך הצומת.
- מעדכנת את המיקום הגרפי שלו.
- מדפיסה לוג לטרמינל.

### כאשר מתקבלת `MSG_LEAVING_NODE`

הפונקציה:

- מסמנת שה־traveler נמצא בתנועה.
- מחשבת את זמן התנועה לפי משקל הקשת.
- משחררת את הצומת.
- מאפשרת לבחור את הממתין הבא.

### כאשר מתקבלת `MSG_FINISHED`

הפונקציה:

- מסמנת שה־traveler סיים.
- עוצרת את האנימציה שלו.
- משחררת את צומת היעד.
- מאפשרת לממתין הבא להיכנס.

---

## `startHandler`

```c
static void startHandler(int sig)
{
    (void)sig;
}
```

זו פונקציית טיפול ב־`SIGUSR1`.

הפונקציה עצמה ריקה, אבל קבלת הסיגנל גורמת ל־`pause()` של הילד להסתיים.

הזרימה:

```text
הילד מגדיר startHandler
→ הילד נכנס ל-pause()
→ האב שולח SIGUSR1
→ startHandler מופעל
→ pause() מסתיים
→ הילד ממשיך לעבוד
```

חשוב:

```c
kill(pid, SIGUSR1);
```

לא בהכרח הורג את התהליך.

במקרה הזה `kill` רק שולחת signal.

---

## `childMain`

```c
static void childMain(const char *filename,
                      int travelerIndex,
                      int src,
                      int dest,
                      int writeFd,
                      int approvalFd)
```

זו הפונקציה שרצה בתוך כל תהליך בן.

היא:

1. מגדירה handler עבור `SIGUSR1`.
2. מחכה ב־`pause()` ללחיצה על Play.
3. טוענת את הגרף בעצמה.
4. מחשבת Dijkstra מהמקור ליעד שלה.
5. עוברת על הצמתים במסלול.
6. שולחת `MSG_REQUEST_NODE`.
7. מחכה לאישור מהאב.
8. שולחת `MSG_ENTERED_NODE`.
9. נשארת בצומת למשך הזמן המוגדר.
10. שולחת `MSG_LEAVING_NODE`.
11. ישנה לפי משקל הקשת.
12. בסוף שולחת `MSG_FINISHED`.
13. משחררת זיכרון וסוגרת pipes.

אם משימה קשורה להתנהגות תהליכי הבנים, זו בדרך כלל הפונקציה המרכזית.

---

## `spawnTravelers`

```c
static int spawnTravelers(Traveler *travelers,
                          int numTravelers,
                          const char *filename)
```

יוצרת תהליך בן לכל traveler.

לכל traveler היא:

1. מאפסת את המצב שלו.
2. יוצרת pipe של ילד אל אב.
3. יוצרת control pipe של אב אל ילד.
4. מבצעת `fork()`.
5. בתוך הילד מפעילה את `childMain`.
6. בתוך האב שומרת את ה־PID.
7. סוגרת קצוות pipe שאינם נחוצים.
8. מגדירה את pipe הקריאה כ־non-blocking.

ה־non-blocking חשוב כדי שלולאת ה־GUI לא תיתקע כאשר אין הודעה חדשה.

---

## `runGraphVisualizer`

```c
void runGraphVisualizer(const char *filename,
                        SchedulerType scheduler)
```

זו הפונקציה הראשית של `GraphVisual.c`.

היא מחברת את כל חלקי הסימולציה.

היא:

1. טוענת את הגרף.
2. מאתחלת תורים לכל הצמתים.
3. קוראת את מספר ה־travelers.
4. קוראת מקור ויעד לכל traveler.
5. מחשבת תוצאות Dijkstra לצורכי תצוגה.
6. יוצרת את תהליכי הבנים.
7. טוענת את הגרף לציור.
8. פותחת חלון raylib.
9. קוראת הודעות מהילדים בכל frame.
10. מפעילה את `dispatchReadyNodes`.
11. מנקה ילדים שסיימו.
12. מטפלת ב־Play.
13. מטפלת ב־Pause ו־Resume בעזרת `SIGSTOP` ו־`SIGCONT`.
14. מטפלת ב־Reset.
15. מציירת קשתות, צמתים ו־travelers.
16. מציגה את מצב כל traveler.
17. מציגה הודעה כאשר כולם סיימו.
18. מנקה תהליכים, pipes וזיכרון בסיום.

---

# 2. פונקציות של `Dijkstra.c`

## מה המטרה של `Dijkstra.c`?

`Dijkstra.c` אחראי על:

- יצירת הגרף בזיכרון.
- שמירת הגרף כרשימות שכנים.
- טעינת הגרף מקובץ.
- חישוב המסלול הקצר ביותר.
- הדפסת התוצאה.
- שחרור זיכרון.
- מציאת קובץ `Graph.txt`.

---

## `skipCommentsAndWS`

```c
static void skipCommentsAndWS(FILE *fp)
```

מדלגת על רווחים ועל שורות שמתחילות ב־`#`.

כך אפשר להוסיף הערות לקובץ הקלט בלי לפגוע בקריאה.

---

## `readInt`

```c
static int readInt(FILE *fp, int *out)
```

קוראת מספר שלם מתוך הקובץ.

לפני הקריאה היא מפעילה את `skipCommentsAndWS`.

היא מחזירה ערך המציין אם הקריאה הצליחה.

---

## `fileReadable`

```c
static int fileReadable(const char *path)
```

בודקת האם הנתיב:

- אינו `NULL`.
- קיים.
- מצביע על קובץ רגיל.
- ניתן לקריאה.

---

## `getExecutableDir`

```c
static int getExecutableDir(char *out, size_t outsize)
```

מוצאת את התיקייה שבה נמצא קובץ ההרצה.

ב־macOS היא משתמשת ב־`_NSGetExecutablePath`.

ב־Linux היא קוראת את:

```text
/proc/self/exe
```

התוצאה משמשת כדי לחפש את `Graph.txt` ליד קובץ ההרצה.

---

## `resolveGraphPath`

```c
char *resolveGraphPath(const char *userArg)
```

מנסה למצוא את קובץ הגרף לפי הסדר:

1. הנתיב שהמשתמש נתן.
2. `Graph.txt` בתיקייה הנוכחית.
3. `Graph.txt` ליד קובץ ההרצה.
4. `Graph.txt` בתוך `SOURCE_DIR`.

כאשר נמצא קובץ, מוחזר עותק של הנתיב.

מי שקורא לפונקציה צריך לשחרר אותו:

```c
free(path);
```

---

## `createGraph`

```c
Graph *createGraph(int n)
```

יוצרת גרף עם `n` צמתים.

היא:

1. מקצה מבנה `Graph`.
2. שומרת את מספר הצמתים.
3. מקצה מערך של רשימות שכנים.
4. מאתחלת כל רשימה ל־`NULL`.

---

## `addEdge`

```c
void addEdge(Graph *g, int from, int to, int w)
```

מוסיפה קשת מכוונת מ־`from` אל `to` במשקל `w`.

לדוגמה:

```text
0 2 5
```

מייצג קשת:

```text
0 → 2
```

במשקל 5.

---

## `freeGraph`

```c
void freeGraph(Graph *g)
```

משחררת את כל הזיכרון של הגרף.

היא:

1. עוברת על כל רשימת שכנים.
2. משחררת כל קשת.
3. משחררת את מערך רשימות השכנים.
4. משחררת את מבנה הגרף.

---

## `minDist`

```c
static int minDist(int dist[], int visited[], int n)
```

מחזירה את הצומת:

- שעדיין לא ביקרנו בו.
- שיש לו את המרחק הזמני הקטן ביותר.

זו פונקציית עזר של Dijkstra.

---

## `dijkstra`

```c
DijkstraResult dijkstra(Graph *g, int src, int dest)
```

מחשבת את המסלול הקצר ביותר מ־`src` אל `dest`.

היא משתמשת בשלושה מערכים:

```text
dist
```

שומר את המרחק הקצר ביותר הידוע לכל צומת.

```text
visited
```

מסמן אילו צמתים כבר עובדו.

```text
parent
```

שומר מאיזה צומת הגענו לכל צומת כדי לבנות את המסלול בסוף.

הפונקציה:

1. בודקת שהקלט חוקי.
2. מקצה את המערכים.
3. מאתחלת את המרחקים לערך גדול.
4. מגדירה את מרחק המקור ל־0.
5. בוחרת בכל שלב צומת בעזרת `minDist`.
6. מעדכנת את מרחקי השכנים.
7. בודקת האם יש מסלול ליעד.
8. בונה את המסלול בעזרת `parent`.
9. מחזירה `DijkstraResult`.

התוצאה כוללת:

```text
found
```

האם נמצא מסלול.

```text
distance
```

המשקל הכולל.

```text
path
```

מערך הצמתים במסלול.

```text
pathLength
```

מספר הצמתים במסלול.

---

## `freeDijkstraResult`

```c
void freeDijkstraResult(DijkstraResult *r)
```

משחררת את מערך המסלול שבתוך תוצאת Dijkstra ומאפסת את השדות.

---

## `printDijkstraResult`

```c
void printDijkstraResult(const DijkstraResult *r)
```

מדפיסה את המסלול ואת המשקל הכולל.

לדוגמה:

```text
0 -> 2 -> 5
10
```

אם אין מסלול:

```text
No path found
```

---

## `loadGraphFromFile`

```c
Graph *loadGraphFromFile(const char *filename,
                         int *src,
                         int *dest)
```

טוענת קובץ בפורמט של תוכנית Dijkstra בטרמינל.

הפורמט:

```text
N M
from to weight
...
src dest
```

היא:

1. פותחת את הקובץ.
2. קוראת מספר צמתים וקשתות.
3. יוצרת גרף.
4. קוראת את כל הקשתות.
5. בודקת שהערכים חוקיים.
6. קוראת מקור ויעד.
7. מחזירה את הגרף.

---

## `loadGraphOnly`

```c
Graph *loadGraphOnly(const char *filename, FILE **out_fp)
```

טוענת רק את חלק הגרף ומשאירה את הקובץ פתוח.

היא מחזירה את ה־`FILE*` דרך `out_fp`, כדי ש־`GraphVisual.c` ימשיך לקרוא את מספר ה־travelers ואת המקור והיעד שלהם.

---

# 3. פונקציות של `main.c`

## מה המטרה של `main.c`?

`main.c` הוא נקודת הכניסה של הסימולטור.

הוא אחראי על:

- בדיקת הארגומנטים.
- בחירת FCFS או SJF.
- מציאת קובץ הגרף.
- הפעלת `runGraphVisualizer`.

---

## `printUsage`

```c
static void printUsage(const char *progName)
```

מדפיסה למשתמש כיצד להריץ את התוכנית.

```bash
./sim -schd fcfs Graph.txt
./sim -schd sjf Graph.txt
```

הפונקציה מופעלת כאשר:

- מספר הארגומנטים שגוי.
- הדגל `-schd` חסר.
- שם ה־scheduler אינו חוקי.

---

## `main`

```c
int main(int argc, char *argv[])
```

זו נקודת ההתחלה של התוכנית.

עבור הפקודה:

```bash
./sim -schd fcfs Graph.txt
```

הערכים הם:

```text
argv[0] = ./sim
argv[1] = -schd
argv[2] = fcfs
argv[3] = Graph.txt
argc    = 4
```

לכן נבדק:

```c
if (argc != 4)
```

הפונקציה:

1. בודקת שיש 4 ארגומנטים.
2. בודקת ש־`argv[1]` הוא `-schd`.
3. בוחרת FCFS או SJF לפי `argv[2]`.
4. לוקחת את נתיב הקובץ מ־`argv[3]`.
5. מפעילה את `resolveGraphPath`.
6. מפעילה את:

```c
runGraphVisualizer(path, scheduler);
```

7. משחררת את הנתיב עם:

```c
free(path);
```

8. מסיימת עם `return 0`.

---

# פורמט `Graph.txt`

הפורמט של קובץ הסימולציה:

```text
# graph definition
N M
from to weight
from to weight
...

# travelers
T
src dest
src dest
...
```

דוגמה:

```text
6 8
0 1 4
0 2 2
1 2 5
1 3 10
2 4 3
4 3 4
3 5 11
4 5 8

3
0 5
1 4
2 3
```

המשמעות:

- יש 6 צמתים.
- יש 8 קשתות.
- כל קשת נכתבת בפורמט `from to weight`.
- יש 3 travelers.
- לכל traveler יש `src dest`.

---

# סיכום מהיר

```text
main.c
= ארגומנטים, בחירת scheduler והפעלת הסימולציה.

Dijkstra.c
= טעינת גרף וחישוב המסלול הקצר.

GraphVisual.c
= GUI, תהליכים, pipes, signals, תורים ו-FCFS/SJF.
```

```text
runGraphVisualizer
= מנהלת את כל הסימולציה.

spawnTravelers
= יוצרת fork ו-pipes.

childMain
= הקוד שרץ בכל תהליך בן.

processTravelerMessages
= האב מקבל הודעות מהילדים.

pickNextTravelerIndex
= בחירת FCFS או SJF.

tryDispatchNextTraveler
= אישור כניסה לצומת.

dijkstra
= חישוב המסלול הקצר ביותר.

resolveGraphPath
= מציאת קובץ הגרף.

main
= קריאת ארגומנטים והפעלת ה-GUI.
```

---

# פקודות חיפוש שימושיות במבחן

```bash
grep -n "childMain" GraphVisual.c
grep -n "spawnTravelers" GraphVisual.c
grep -n "processTravelerMessages" GraphVisual.c
grep -n "pickNextTravelerIndex" GraphVisual.c
grep -n "tryDispatchNextTraveler" GraphVisual.c
grep -n "SIG" GraphVisual.c
grep -n "dijkstra" Dijkstra.c
grep -n "schd" main.c
```

---

# העלאת הקובץ ל־GitHub בענף `master`

```bash
git switch master
git pull origin master
git add Project_Functions_Guide.md
git commit -m "add project functions guide"
git push origin master
```

לבדיקה:

```bash
git status
```
