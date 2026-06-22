# דף עזר

המסמך הזה מרכז בצורה מסודרת את כל מה שחשוב לדעת על הפרויקט,
במיוחד אם תעבוד על מחשב אחר בסביבת לינוקס ולא על המחשב האישי שלך.

---

## 1. מה חשוב להבין על סביבת העבודה

הפרויקט שלכם נכתב על מק, אבל הוא מבוסס על יכולות POSIX כמו:

- `fork`
- `pipe`
- `kill`
- `waitpid`
- signals

לכן בפועל הוא **יותר מתאים ללינוקס מאשר ל-Windows**.

כלומר:
- המעבר ממאק ללינוקס הוא טבעי יחסית
- רוב הלוגיקה תישאר זהה
- מה שישתנה זה בעיקר סביבת העבודה והדרך שבה תפתח/תערוך/תריץ את הקוד

---

## 2. דרך עבודה מומלצת

תעבוד לפי הסדר הזה:

1. למשוך את הפרויקט
2. לוודא שהוא נבנה ורץ
3. להבין לאיזה חלק בקוד המשימה שייכת
4. לשנות כמה שפחות
5. לבדוק
6. לעשות `commit` ו-`push`

---

## 3. הפקודות החשובות ביותר בלינוקס

## משיכת הפרויקט

עם SSH:
```bash
git clone git@github.com:DvirBarOn/Linux_Project.git
cd Linux_Project
```

או עם HTTPS:
```bash
git clone https://github.com/DvirBarOn/Linux_Project.git
cd Linux_Project
```

---

## עדכון הפרויקט

```bash
git checkout master
git pull
git fetch --tags
```

---

## לראות מה יש בתיקייה

```bash
ls
```

---

## בנייה

```bash
make clean
make milestone7
```

אם צריך milestone אחר:

```bash
make milestone1
make milestone4
make milestone7
```

---

## הרצה

ל־FCFS:
```bash
./sim -schd fcfs Graph.txt
```

ל־SJF:
```bash
./sim -schd sjf Graph.txt
```

---

## בדיקת מצב git

```bash
git status
```

---

## commit + push

```bash
git add .
git commit -m "implement exam task"
git push
```

---

## לראות תגיות

```bash
git tag
git fetch --tags
```

---

## לעבור ל-tag של final אם צריך

```bash
git checkout final
```

ולחזור ל-master:
```bash
git checkout master
```

---

## 4. איך לערוך קבצים בלינוקס

אם אין לך IDE נוח, הכי פשוט לעבוד עם `nano`.

לדוגמה:
```bash
nano GraphVisual.c
```

פקודות בסיסיות בתוך `nano`:
- `Ctrl + W` — חיפוש
- `Ctrl + O` — שמירה
- `Enter` — אישור שם הקובץ
- `Ctrl + X` — יציאה

זה מספיק לחלוטין לעבודה מהירה על הפרויקט.

---

## 5. מפת הקבצים הכי חשובה לזכור

## `Dijkstra.c`
כאן נמצאים:
- טעינת גרף
- shortest path
- Dijkstra
- parsing של חלק מהקלט

אם המשימה קשורה ל:
- shortest path
- graph loading
- path calculation

כנראה תיגע כאן.

---

## `Dijkstra.h`
כאן נמצאים:
- structs משותפים
- declarations של פונקציות

אם הוספת פונקציה או שדה חדש שצריך להיות משותף, תיגע גם כאן.

---

## `main.c`
כאן נמצאים:
- ארגומנטים של שורת הפקודה
- בחירת scheduler
- הקריאה ל־`runGraphVisualizer(...)`

אם המשימה קשורה ל:
- flags
- אופן ההרצה
- ברירת מחדל של scheduler

כנראה תיגע כאן.

---

## `GraphVisual.c`
זה הקובץ הכי חשוב במצב GUI.

כאן נמצאים:
- GUI
- travelers
- processes
- signals
- pipes
- scheduling
- animation
- node queues
- Play/Pause/Reset

אם המשימה קשורה ל:
- תהליכים
- signals
- waiting
- scheduler
- GUI
- movement
- node access
- parent / child behavior

כמעט בטוח שזה הקובץ שצריך לשנות.

---

## `Makefile`
כאן נמצאות פקודות build.

אם יש בעיית קימפול או target, תבדוק כאן.

---

## `README.md`
תיעוד והוראות הרצה.

---

## 6. איך לזהות מהר איפה צריך לשנות

## אם המשימה על shortest path
תחפש ב:
- `Dijkstra.c`
- `Dijkstra.h`

---

## אם המשימה על CLI / flags
תחפש ב:
- `main.c`

---

## אם המשימה על processes / signals / pipes / scheduling / GUI
תחפש ב:
- `GraphVisual.c`

---

## 6. פקודות grep שמאוד יעזרו בעבודה

## משימה על signals
```bash
grep -n "SIG" GraphVisual.c
grep -n "kill" GraphVisual.c
grep -n "waitpid" GraphVisual.c
grep -n "childMain" GraphVisual.c
```

---

## משימה על scheduler
```bash
grep -n "SchedulerType" GraphVisual.c
grep -n "pickNextTravelerIndex" GraphVisual.c
grep -n "tryDispatchNextTraveler" GraphVisual.c
grep -n "processTravelerMessages" GraphVisual.c
```

---

## משימה על queues / node access
```bash
grep -n "NodeQueue" GraphVisual.c
grep -n "MSG_REQUEST_NODE" GraphVisual.c
grep -n "MSG_ENTERED_NODE" GraphVisual.c
grep -n "MSG_LEAVING_NODE" GraphVisual.c
```

---

## משימה על shortest path
```bash
grep -n "dijkstra" Dijkstra.c
```

---

## משימה על הרצה / flags
```bash
grep -n "schd" main.c
grep -n "runGraphVisualizer" main.c
```

---

## 8. איך לחשוב לפי סוג משימה

## דוגמה: משימה על תהליכים וסיגנלים
אם כתוב לך למשל:

> לשנות את ההתנהגות של תהליכי הבנים כך שכל תהליך בן יקבל `SIGUSR1` במקום סיגנל שהורג אותו, ובקבלת `SIGUSR1` ידפיס כמה זמן ישן ורק לאחר מכן ייצא.

אז המסקנה היא:
- זה קשור ל־child process behavior
- זה קשור ל־signals
- זה קשור לאופן יציאה של הילדים

ולכן צריך לחפש ב־`GraphVisual.c`.

הגישה הנכונה תהיה:
1. לחפש איפה הילדים נוצרים
2. לחפש איפה הילד מקבל signals
3. לחפש איפה האבא שולח signals
4. לחפש איפה הילד יוצא

---

## 8. איך לחלק את הזמן

## 0–10 דקות
- לקרוא את המשימה
- להבין לאיזה קובץ היא שייכת
- להריץ baseline

## 10–35 דקות
- לממש שינוי מינימלי והגיוני
- לשמור
- לבנות
- לבדוק

## 35–50 דקות
- לתקן שגיאות
- לשפר ניסוח/סדר/קריאות

## 50–60 דקות
- `git status`
- `git add`
- `git commit`
- `git push`

אסור להיגרר עד הדקה האחרונה בלי commit.

---

## 10. מה לעשות קודם לפי שיטת הניקוד

בגלל ש־35% זה על זיהוי נכון של המקום בקוד, תעבוד כך:

1. קודם תחליט **איפה** השינוי אמור להיות
2. רק אחר כך תכתוב קוד
3. תשתדל לשנות כמה שפחות קבצים
4. אל תפרק חצי פרויקט בשביל משימה קטנה

עדיף:
- שינוי קטן
- ברור
- מקומי

מאשר שינוי גדול שמסכן את כל הבנייה.

---

## 11. סדר עבודה מומלץ בפועל

```bash
git clone https://github.com/DvirBarOn/Linux_Project.git
cd Linux_Project
git checkout master
git pull
make clean
make milestone7
./sim -schd fcfs Graph.txt
git status
nano GraphVisual.c
make milestone7
./sim -schd fcfs Graph.txt
git add .
git commit -m "implement exam task"
git push
```

---

## 12. מה לבדוק אם משהו לא עובד

## אם לא נבנה
תבדוק:
- `Makefile`
- האם יש typo
- האם שכחת `;`
- האם struct/function declaration תואמים ל-header

---

## אם traveler לא זז
תבדוק:
- `MSG_LEAVING_NODE`
- `moveStartTime`
- `edgeDuration`
- animation logic ב־`GraphVisual.c`

---

## אם traveler לא נכנס לצומת
תבדוק:
- `MSG_REQUEST_NODE`
- `NodeQueue`
- `enqueueTraveler(...)`
- `tryDispatchNextTraveler(...)`
- `sendEnterApproval(...)`
- `waitForEnterApproval(...)`

---

## אם scheduler לא מתנהג נכון
תבדוק:
- `SchedulerType`
- `pickNextTravelerIndex(...)`
- `arrivalOrder`
- `nextEdgeWeight`

---

## אם Reset לא עובד
תבדוק:
- `cleanupTravelerProcess(...)`
- `resetTravelerState(...)`
- `spawnTravelers(...)`

---

## 13. משפטי מפתח שכדאי לזכור

אם אתה תקוע, תשאל את עצמך:

- זה shortest path? → `Dijkstra.c`
- זה flags / CLI? → `main.c`
- זה GUI / travelers / scheduling / signals / processes? → `GraphVisual.c`

זו המפה הכי חשובה.

---

## 14. צ'יטשיט קצר במיוחד

## משיכה ובנייה
```bash
git clone https://github.com/DvirBarOn/Linux_Project.git
cd Linux_Project
git checkout master
git pull
make clean
make milestone7
```

## הרצה
```bash
./sim -schd fcfs Graph.txt
./sim -schd sjf Graph.txt
```

## עריכה
```bash
nano GraphVisual.c
```

## חיפושים
```bash
grep -n "SIG" GraphVisual.c
grep -n "kill" GraphVisual.c
grep -n "SchedulerType" GraphVisual.c
grep -n "NodeQueue" GraphVisual.c
grep -n "dijkstra" Dijkstra.c
```

## גיט
```bash
git status
git add .
git commit -m "implement exam task"
git push
```

---

## 15. מסקנה סופית

אתה לא צריך לזכור את כל הפרויקט בעל פה.

אתה כן צריך לזכור:
- איך מושכים את הריפו
- איך בונים
- איך מריצים
- מה עושה כל קובץ מרכזי
- איך לזהות מהר איפה המשימה אמורה להיות
- לא לשכוח `commit` ו־`push`

אם תעבוד לפי הסדר הזה, גם על מחשב לינוקס זר תדע להתנהל טוב עם הפרויקט.
---

## 15. סט משימות דוגמה לתרגול (משימות 1–5)

המטרה של הסעיף הזה היא לעזור לך לתרגל מראש צורת חשיבה מסודרת לעבודה על הפרויקט:
- לזהות מהר את הקובץ הנכון
- להבין למה דווקא שם צריך לשנות
- לדעת מה לחפש
- ולבנות פתרון קטן, ברור והגיוני

---

### משימה 1 — שינוי התנהגות סיגנל לילדים

#### דוגמה לניסוח
ב־milestone 4/5/6/7, במקום שכאשר ההורה מסיים traveler הוא ישלח סיגנל שמסיים אותו מיד, כל child יקבל `SIGUSR1`, ידפיס הודעה, ואז ייצא.

#### איפה כנראה משנים
- `GraphVisual.c`

#### למה דווקא שם
כי שם נמצאים:
- יצירת ה־children עם `fork()`
- שליחת signals
- הטיפול בהתנהגות של child
- `childMain(...)`
- `cleanupTravelerProcess(...)`

#### מה לחפש
```bash
grep -n "SIG" GraphVisual.c
grep -n "kill" GraphVisual.c
grep -n "childMain" GraphVisual.c
```

#### איך לגשת
1. למצוא איפה ההורה שולח סיגנל לילד
2. למצוא איפה הילד מגדיר signal handler
3. להוסיף handler לסיגנל המבוקש
4. ב־handler להדפיס את ההודעה
5. לגרום לילד לצאת רק אחרי ההדפסה

#### מה חשוב להסביר
- שהמשימה שייכת ל־`GraphVisual.c`
- שהקוד הזה מנהל את המודל parent/child
- שההתערבות צריכה להיות רק סביב סיגנלים ויציאה, בלי לשבור את שאר הלוגיקה

---

### משימה 2 — ברירת מחדל ל־FCFS אם לא הועבר `-schd`

#### דוגמה לניסוח
אם המשתמש מריץ `./sim Graph.txt` בלי `-schd`, התוכנית תשתמש אוטומטית ב־`FCFS`.

#### איפה כנראה משנים
- `main.c`

#### למה דווקא שם
כי שם מתבצע parsing של הארגומנטים משורת הפקודה.

#### מה לחפש
```bash
grep -n "schd" main.c
grep -n "runGraphVisualizer" main.c
grep -n "argc" main.c
```

#### איך לגשת
1. למצוא איפה בודקים את מספר הארגומנטים
2. למצוא איפה מודפס `Usage`
3. לשנות את הלוגיקה כך שאם לא התקבל `-schd`, משתמשים ב־`FCFS`
4. להשאיר תמיכה רגילה ב־`-schd fcfs` וב־`-schd sjf`

#### מה חשוב להסביר
- שלא צריך לגעת בכלל ב־`GraphVisual.c`
- שזו משימת CLI / parsing ולכן המקום הנכון הוא `main.c`

---

### משימה 3 — לשנות את זמן השהייה בצומת

#### דוגמה לניסוח
כל traveler יישאר בתוך צומת 2 שניות במקום שנייה אחת.

#### איפה כנראה משנים
- `GraphVisual.c`

#### למה דווקא שם
כי שם מוגדר קבוע זמן השהייה בצומת, ושם גם מתבצעת ההשהיה בפועל.

#### מה לחפש
```bash
grep -n "NODE_STAY_SECONDS" GraphVisual.c
grep -n "sleepSeconds" GraphVisual.c
```

#### איך לגשת
1. למצוא את הקבוע שמגדיר את זמן השהייה
2. לשנות את הערך
3. לקמפל ולהריץ
4. לוודא שהשהייה באמת השתנתה

#### מה חשוב להסביר
- שזה שינוי קטן ומקומי
- שזה לא נוגע ל־scheduler ולא ל־Dijkstra
- שזה משפיע על הסימולציה עצמה בלבד

---

### משימה 4 — לשנות את ההגדרה של SJF

#### דוגמה לניסוח
במקום ש־`SJF` יבחר לפי משקל הקשת הבאה, הוא יבחר לפי סכום שתי הקשתות הבאות במסלול, אם קיימות.

#### איפה כנראה משנים
- `GraphVisual.c`

#### למה דווקא שם
כי אצלכם:
- ה־child מחשב איזה מידע לשלוח להורה
- ה־parent בוחר את traveler הבא לפי אותו מידע
- `SJF` כבר קיים שם

#### מה לחפש
```bash
grep -n "nextEdgeWeight" GraphVisual.c
grep -n "pickNextTravelerIndex" GraphVisual.c
grep -n "MSG_REQUEST_NODE" GraphVisual.c
grep -n "childMain" GraphVisual.c
```

#### איך לגשת
1. להבין מאיפה מגיע `nextEdgeWeight`
2. להחליט אם צריך שדה חדש או שאפשר להשתמש בשדה הקיים עם משמעות חדשה
3. לשנות את החישוב בצד ה־child
4. להשאיר את בחירת המועמד בצד ה־parent דומה ככל האפשר

#### מה חשוב להסביר
- שהמקום הנכון הוא עדיין `GraphVisual.c`
- שזו משימת scheduling ולא shortest path "טהור"
- שמומלץ לשמור על מינימום שינויים במבנה הקיים

---

### משימה 5 — להוסיף הדפסה של החלטת scheduler

#### דוגמה לניסוח
בכל פעם שההורה מחליט מי נכנס לצומת, יודפס:
`Scheduler selected T2 for node 3`

#### איפה כנראה משנים
- `GraphVisual.c`

#### למה דווקא שם
כי שם מתקבלת בפועל ההחלטה איזה traveler הבא ייכנס לצומת.

#### מה לחפש
```bash
grep -n "pickNextTravelerIndex" GraphVisual.c
grep -n "tryDispatchNextTraveler" GraphVisual.c
grep -n "sendEnterApproval" GraphVisual.c
```

#### איך לגשת
1. למצוא את הפונקציה שבה נשלחת ההרשאה להיכנס לצומת
2. לזהות את הרגע שבו כבר ידוע מי נבחר
3. להוסיף `printf(...)` במקום הזה
4. לוודא שההדפסה כוללת לפחות:
   - scheduler
   - node
   - traveler שנבחר

#### מה חשוב להסביר
- שלא מוסיפים את ההדפסה במקום “בערך נכון”
- אלא בדיוק במקום שבו ההחלטה מתקבלת בפועל
- וכך ההדפסה באמת משקפת את הלוגיקה של המערכת

---

## 16. איך לעבוד עם משימות דוגמה כאלה

לכל אחת מהמשימות למעלה, תשתדל לעבוד באותה שיטה:

1. לזהות את סוג המשימה  
   - signals / processes / scheduler / GUI → `GraphVisual.c`
   - flags / CLI → `main.c`
   - shortest path / graph loading → `Dijkstra.c`

2. להריץ `grep -n` על מילות מפתח רלוונטיות

3. למצוא את המקום הכי קטן והכי מדויק לשינוי

4. לשנות מינימום הכרחי

5. לבדוק build והרצה

6. לא לשכוח:
```bash
git status
git add .
git commit -m "implement exam task"
git push
```

---

## 18. מסקנה לגבי משימות 1–5

אם תבין טוב את חמש המשימות האלה, תהיה לך הכנה טובה מאוד לרוב משימות השינוי הסבירות בפרויקט, כי הן יושבות בדיוק על הלב של הפרויקט:

- signals
- processes
- scheduler
- command-line arguments
- behavior של travelers

כלומר, גם אם תקבל משימה קצת שונה, סביר מאוד שהיא תהיה וריאציה על אחת מהן.
