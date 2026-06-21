# Linux_Project Architecture Map

מסמך זה מרכז את הארכיטקטורה של הפרויקט, כדי שיהיה קל להבין:
- מה כל קובץ עושה
- איפה משנים כל התנהגות
- איך המערכת זורמת בזמן ריצה
- איך לגשת לקוד כשצריך להוסיף פיצ'ר או לתקן באג

---

## 1. מבט על

הפרויקט הוא סימולטור של גרף מכוון ומשוקלל עם שני מצבי עבודה עיקריים:

1. **Milestone 1**  
   הרצת Dijkstra בטרמינל.

2. **Milestones 4–7**  
   סימולציית GUI עם מספר travelers, תהליכי child, תקשורת IPC, סנכרון כניסה לצמתים, ואלגוריתמי תזמון (`FCFS`, `SJF`).

בפועל, הפרויקט בנוי סביב 4 קבצים עיקריים:

- `Dijkstra.h`
- `Dijkstra.c`
- `GraphVisual.c`
- `main.c`

וקבצי מעטפת:

- `Dijkstra_main.c`
- `Makefile`
- `CMakeLists.txt`
- `README.md`
- `Graph.txt` / קבצי קלט נוספים

---

## 2. מפת קבצים

### `Dijkstra.h`
קובץ header מרכזי.

### מה יש בו
- מבני נתונים של הגרף
- מבנה `Edge`
- מבנה `Graph`
- מבנה `DijkstraResult`
- הצהרות לפונקציות עזר, טעינת גרף, Dijkstra, שחרור זיכרון וכו'

### מתי נוגעים בו
- כשמוסיפים פונקציה ציבורית חדשה שצריכה להיות זמינה בין קבצים
- כשמשנים struct שצריך להיות משותף ל־`Dijkstra.c`, `GraphVisual.c`, `main.c`

### לא לגעת אם
- השינוי הוא פנימי רק ל־`GraphVisual.c`
- מדובר בפונקציות `static`

---

### `Dijkstra.c`
המנוע האלגוריתמי של הפרויקט.

### אחריות
- טעינת גרף מהקלט
- שמירת adjacency list
- מימוש Dijkstra
- פונקציות עזר:
  - `loadGraphOnly(...)`
  - `dijkstra(...)`
  - `freeGraph(...)`
  - `freeDijkstraResult(...)`
  - `resolveGraphPath(...)`
  - פונקציות parsing / הקצאות / ניקוי

### מתי נוגעים בו
- אם צריך לשנות איך הגרף נטען מהקלט
- אם צריך לשנות את מימוש Dijkstra
- אם צריך להוסיף לוגיקה של shortest path או פורמט חדש לטעינה

### לא לגעת אם
- השינוי הוא GUI
- השינוי הוא Play/Pause/Reset
- השינוי הוא תזמון צמתים או traveler animation

---

### `Dijkstra_main.c`
נקודת כניסה פשוטה עבור Milestone 1.

### אחריות
- מריץ את מצב הטרמינל
- קורא גרף
- מפעיל Dijkstra
- מדפיס תוצאה למסך

### מתי נוגעים בו
- רק אם משנים את אופן ההרצה של milestone 1
- אם משנים את פורמט ההדפסה של milestone 1

### בדרך כלל
ברוב הפיצ'רים החדשים לא צריך לגעת בו.

---

### `main.c`
נקודת הכניסה של הסימולטור.

### אחריות
- קריאת ארגומנטים משורת הפקודה
- בחירת scheduler (`fcfs` / `sjf`)
- מציאת קובץ הגרף
- קריאה ל־`runGraphVisualizer(...)`

### מתי נוגעים בו
- אם מוסיפים דגל חדש ל־CLI
- אם מוסיפים mode חדש להרצה
- אם רוצים לשנות syntax של הפקודה

### דוגמאות
אם רוצים להוסיף למשל:
- `-speed fast`
- `-log verbose`
- `-schd rr`

זה המקום הראשון לשנות.

---

### `GraphVisual.c`
זה הקובץ הכי חשוב במצב GUI.

### אחריות
- ציור הגרף עם raylib
- ניהול travelers
- ניהול child processes
- ניהול pipe/control pipes
- קבלת הודעות מהילדים
- לוגיקת כניסה/יציאה לצמתים
- ניהול queue לכל node
- מימוש `FCFS` ו־`SJF`
- Play / Pause / Resume / Reset
- לוגיקת אנימציה על קשתות

### מתי נוגעים בו
כמעט כל שינוי התנהגותי בסימולטור יקרה כאן.

---

### `Graph.txt`
קובץ קלט ברירת מחדל.

### אחריות
- מתאר את הגרף
- מתאר את רשימת ה־travelers

### מתי נוגעים בו
- לבדיקות
- להכנת demo
- כשצריך test case חדש

---

### `Makefile`
אחראי על build ידני.

### מתי נוגעים בו
- אם מוסיפים target חדש
- אם משנים שמות בינאריים
- אם צריך עוד milestone target
- אם יש ספריות חדשות לקימפול

---

### `CMakeLists.txt`
אחראי על build עם CMake.

### מתי נוגעים בו
- אם מוסיפים קבצים חדשים לפרויקט
- אם משנים אופן linking
- אם צריך target חדש ב־CMake

---

### `README.md`
מסמך תיעוד הפרויקט.

### מתי נוגעים בו
- כשמתווסף milestone חדש
- כשמשתנה אופן ההרצה
- כשצריך לעדכן build/run instructions

---

## 3. זרימת המערכת בזמן ריצה

## מצב 1: milestone 1
1. `Dijkstra_main.c`
2. טוען גרף דרך `Dijkstra.c`
3. מפעיל `dijkstra(...)`
4. מדפיס shortest path

## מצב 2: GUI
1. `main.c` קורא את הארגומנטים
2. מזהה scheduler
3. קורא ל־`runGraphVisualizer(filename, scheduler)`
4. `GraphVisual.c`:
   - טוען את הגרף
   - קורא traveler list
   - יוצר child process לכל traveler
   - פותח pipes
   - מנהל GUI loop
5. כל child:
   - טוען גרף בעצמו
   - מחשב shortest path
   - שולח בקשות לכניסה לצומת
6. האב:
   - מקבל הודעות
   - מחליט מי נכנס לצומת
   - שולח אישור דרך control pipe
   - מעדכן animation ו־state

---

## 4. ארכיטקטורת runtime

### Parent process
האבא אחראי על:
- חלון raylib
- UI
- queue של כל node
- scheduler logic
- dispatch לכניסה לצומת
- ניהול pause/resume/reset
- קריאת הודעות מכל הילדים
- ציור current state

### Child process
כל ילד אחראי על:
- shortest path של traveler אחד
- שליחת `request node`
- המתנה לאישור כניסה
- שליחת `entered node`
- המתנה בצומת
- שליחת `leaving node`
- תנועה לאורך המסלול
- שליחת `finished`

---

## 5. איפה משנים כל דבר

## A. שינוי אלגוריתם shortest path
לך ל:
- `Dijkstra.c`
- `Dijkstra.h`

אם למשל תרצה:
- Bellman-Ford
- A*
- מסלול עם constraints

זה המקום.

---

## B. שינוי פורמט הקלט
לך ל:
- `Dijkstra.c` אם זה קשור לגרף
- `GraphVisual.c` אם זה קשור ל־travelers section

אם לדוגמה תוסיף:
- priority לכל traveler
- color בקלט
- speed שונה לכל traveler

אז תעדכן parsing שם.

---

## C. שינוי scheduler
לך ל:
- `main.c` כדי להוסיף CLI option
- `GraphVisual.c` כדי לממש את האלגוריתם

חפש אזורים של:
- `SchedulerType`
- `pickNextTravelerIndex(...)`
- `tryDispatchNextTraveler(...)`
- `processTravelerMessages(...)`

אם רוצים להוסיף scheduler חדש, למשל:
- `RR`
- `Priority`
- `LJF`

צריך:
1. להוסיף enum
2. לעדכן parsing ב־`main.c`
3. לעדכן בחירת next traveler ב־`GraphVisual.c`
4. לעדכן README

---

## D. שינוי התנהגות node access
לך ל:
- `GraphVisual.c`

אם רוצים לשנות:
- כמה זמן traveler נשאר בתוך צומת
- האם מותר יותר מ־1 traveler בתוך node
- איך traveler מחכה לפני כניסה
- האם queue מתנהג אחרת

חפש:
- `NODE_STAY_SECONDS`
- `NodeQueue`
- `WaitingTraveler`
- `MSG_REQUEST_NODE`
- `MSG_ENTERED_NODE`
- `MSG_LEAVING_NODE`
- `MSG_FINISHED`

---

## E. שינוי animation / UI
לך ל:
- `GraphVisual.c`

אם רוצים לשנות:
- צבעים
- גודל nodes
- גודל travelers
- מיקום labels
- מה כתוב בפאנל צד
- איך נראה traveler שמחכה / זז / בתוך node

חפש:
- constants בחלק העליון
- `drawNode(...)`
- `drawEdge(...)`
- `travelerColor(...)`
- loop הציור בתוך `runGraphVisualizer(...)`

---

## F. שינוי כפתורים Play / Pause / Reset
לך ל:
- `runGraphVisualizer(...)` בתוך `GraphVisual.c`

שם נמצאים:
- `CheckCollisionPointRec(...)`
- `IsMouseButtonPressed(...)`
- לוגיקת pause/resume
- לוגיקת reset

---

## G. שינוי מה מודפס לטרמינל
לך ל:
- `processTravelerMessages(...)` ב־`GraphVisual.c`

אם אתה רוצה:
- להחליף ניסוח
- להוסיף debug
- להוריד debug
- להוסיף לוג של scheduler decision

זה המקום.

---

## H. שינוי מה מוצג ב־README / build commands
לך ל:
- `README.md`
- `Makefile`
- `CMakeLists.txt`

---

## 6. מבני נתונים חשובים

### `Graph`
הייצוג של הגרף.
נמצא ב־`Dijkstra.h`

### `Edge`
קשת ב־adjacency list.

### `DijkstraResult`
התוצאה של shortest path:
- found / not found
- distance
- path
- pathLength

### `Traveler`
state מלא של traveler בצד האב:
- src / dest
- pid
- מצב נוכחי
- currentNode / nextNode
- moving / waiting / inside / finished
- pipeFd / controlPipe
- animation state

### `TravelerMessage`
ההודעות בין ילד לאבא.

### `WaitingTraveler`
entry בתור של node.

### `NodeQueue`
התור של כל צומת:
- מי כרגע בפנים
- מי מחכה
- queueSize

---

## 7. ההודעות בין child ל-parent

ב־milestone 7 הזרימה הלוגית היא בערך:

1. child שולח `MSG_REQUEST_NODE`
2. parent מכניס לתור
3. parent מחליט מי נכנס לפי scheduler
4. parent שולח approval דרך control pipe
5. child שולח `MSG_ENTERED_NODE`
6. child ממתין בתוך node
7. child שולח `MSG_LEAVING_NODE`
8. parent משחרר node ומכניס traveler הבא
9. בסוף child שולח `MSG_FINISHED`

---

## 8. איפה להתחיל כשיש באג

## אם traveler לא זז
בדוק:
- `processTravelerMessages(...)`
- `MSG_LEAVING_NODE`
- `moveStartTime`
- `edgeDuration`

## אם traveler לא נכנס לצומת
בדוק:
- `MSG_REQUEST_NODE`
- `enqueueTraveler(...)`
- `tryDispatchNextTraveler(...)`
- `sendEnterApproval(...)`
- `waitForEnterApproval(...)`

## אם FCFS/SJF לא מתנהגים נכון
בדוק:
- `pickNextTravelerIndex(...)`
- `arrivalOrder`
- `nextEdgeWeight`
- מתי request נכנס לתור
- האם יש collection window

## אם Reset לא עובד
בדוק:
- `cleanupTravelerProcess(...)`
- `resetTravelerState(...)`
- `spawnTravelers(...)`
- אתחול מחדש של `nodeQueues`

## אם יש בעיית build
בדוק:
- `Makefile`
- `CMakeLists.txt`
- paths של raylib
- שמות targets

---

## 9. איך לגשת לשינוי חדש

כשמוסיפים שינוי, תעבוד לפי הסדר הזה:

1. **תבין אם זה שינוי אלגוריתמי או ויזואלי**
   - אלגוריתמי → `Dijkstra.c` / `GraphVisual.c`
   - ויזואלי → `GraphVisual.c`

2. **תבין אם צריך לשנות API בין קבצים**
   - אם כן → גם `Dijkstra.h`

3. **תבדוק אם צריך דגל חדש להרצה**
   - אם כן → `main.c`

4. **תבדוק אם צריך לעדכן build**
   - `Makefile`
   - `CMakeLists.txt`

5. **תעדכן README**
   - תמיד כשיש שינוי שמשפיע על הרצה / milestone / architecture

---

## 10. דוגמאות מהירות

### אני רוצה לשנות את זמן השהייה בצומת
לך ל־`GraphVisual.c`  
חפש:
- `NODE_STAY_SECONDS`

---

### אני רוצה ש־SJF יחשב job length אחרת
לך ל־`GraphVisual.c`  
חפש:
- `nextEdgeWeight`
- `pickNextTravelerIndex(...)`

---

### אני רוצה להוסיף Priority Scheduling
שנה ב:
- `main.c`
- `GraphVisual.c`
- `README.md`

---

### אני רוצה להוסיף צבע שונה לכל traveler לפי הקלט
שנה ב:
- parsing של travelers
- `Traveler`
- `travelerColor(...)` או הצבע שנשמר ב־traveler

---

### אני רוצה שה־GUI יציג עוד מידע
שנה ב:
- `runGraphVisualizer(...)`
- אזור הפאנל השמאלי / טקסטים / labels

---

## 11. build והרצה

### Makefile
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
```

### בדיקת גיט
```bash
git status
git tag
```

---

## 12. המלצה לעבודה עתידית

אם אתה נוגע בפרויקט בעתיד, מומלץ לעבוד ככה:
1. קודם להבין אם השינוי שייך ל־`Dijkstra.c` או ל־`GraphVisual.c`
2. לשנות כמה שפחות קבצים
3. להריץ test case קטן
4. להריץ גם `fcfs` וגם `sjf`
5. לעדכן README רק בסוף
6. לעשות `git status` לפני commit
7. לתייג רק כשבאמת סוגרים milestone

---

## 13. סיכום קצר

אם אתה שואל את עצמך:

- **אלגוריתם מסלול?** → `Dijkstra.c`
- **CLI / flags?** → `main.c`
- **GUI / travelers / scheduling / pipes / animation?** → `GraphVisual.c`
- **types משותפים?** → `Dijkstra.h`
- **build?** → `Makefile` / `CMakeLists.txt`
- **תיעוד?** → `README.md`

זה המיפוי הכי חשוב לזכור.
