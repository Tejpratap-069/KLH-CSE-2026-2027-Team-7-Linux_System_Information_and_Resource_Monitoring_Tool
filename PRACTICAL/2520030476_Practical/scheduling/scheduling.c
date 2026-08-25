#include <stdio.h>

#define MAX 100

struct Process {
    int pid;
    int at;       // Arrival Time
    int bt;       // Burst Time
    int priority;
    int ct;       // Completion Time
    int tat;      // Turnaround Time
    int wt;       // Waiting Time
    int remaining;
};

/* ---------- FCFS ---------- */
void FCFS(struct Process p[], int n)
{
    int time = 0;
    float total_tat = 0, total_wt = 0;

    printf("\n========== FCFS ==========\n");

    for (int i = 0; i < n; i++)
    {
        if (time < p[i].at)
            time = p[i].at;

        time = time + p[i].bt;

        p[i].ct = time;
        p[i].tat = p[i].ct - p[i].at;
        p[i].wt = p[i].tat - p[i].bt;

        total_tat += p[i].tat;
        total_wt += p[i].wt;
    }

    printf("\nPID\tAT\tBT\tCT\tTAT\tWT\n");

    for (int i = 0; i < n; i++)
    {
        printf("P%d\t%d\t%d\t%d\t%d\t%d\n",
               p[i].pid,
               p[i].at,
               p[i].bt,
               p[i].ct,
               p[i].tat,
               p[i].wt);
    }

    printf("\nAverage TAT = %.2f", total_tat / n);
    printf("\nAverage WT  = %.2f\n", total_wt / n);
}

/* ---------- SJF ---------- */
void SJF(struct Process p[], int n)
{
    int completed = 0;
    int time = 0;
    int done[MAX] = {0};

    float total_tat = 0;
    float total_wt = 0;

    printf("\n========== SJF ==========\n");

    while (completed < n)
    {
        int index = -1;
        int shortest = 999999;

        for (int i = 0; i < n; i++)
        {
            if (done[i] == 0 &&
                p[i].at <= time &&
                p[i].bt < shortest)
            {
                shortest = p[i].bt;
                index = i;
            }
        }

        if (index == -1)
        {
            time++;
            continue;
        }

        time = time + p[index].bt;

        p[index].ct = time;
        p[index].tat = p[index].ct - p[index].at;
        p[index].wt = p[index].tat - p[index].bt;

        total_tat += p[index].tat;
        total_wt += p[index].wt;

        done[index] = 1;
        completed++;
    }

    printf("\nPID\tAT\tBT\tCT\tTAT\tWT\n");

    for (int i = 0; i < n; i++)
    {
        printf("P%d\t%d\t%d\t%d\t%d\t%d\n",
               p[i].pid,
               p[i].at,
               p[i].bt,
               p[i].ct,
               p[i].tat,
               p[i].wt);
    }

    printf("\nAverage TAT = %.2f", total_tat / n);
    printf("\nAverage WT  = %.2f\n", total_wt / n);
}

/* ---------- Priority Scheduling ---------- */
void Priority(struct Process p[], int n)
{
    int completed = 0;
    int time = 0;
    int done[MAX] = {0};

    float total_tat = 0;
    float total_wt = 0;

    printf("\n========== PRIORITY ==========\n");
    printf("Lower priority number = Higher priority\n");

    while (completed < n)
    {
        int index = -1;
        int highest = 999999;

        for (int i = 0; i < n; i++)
        {
            if (done[i] == 0 &&
                p[i].at <= time &&
                p[i].priority < highest)
            {
                highest = p[i].priority;
                index = i;
            }
        }

        if (index == -1)
        {
            time++;
            continue;
        }

        time = time + p[index].bt;

        p[index].ct = time;
        p[index].tat = p[index].ct - p[index].at;
        p[index].wt = p[index].tat - p[index].bt;

        total_tat += p[index].tat;
        total_wt += p[index].wt;

        done[index] = 1;
        completed++;
    }

    printf("\nPID\tAT\tBT\tPR\tCT\tTAT\tWT\n");

    for (int i = 0; i < n; i++)
    {
        printf("P%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
               p[i].pid,
               p[i].at,
               p[i].bt,
               p[i].priority,
               p[i].ct,
               p[i].tat,
               p[i].wt);
    }

    printf("\nAverage TAT = %.2f", total_tat / n);
    printf("\nAverage WT  = %.2f\n", total_wt / n);
}

/* ---------- Round Robin ---------- */
void RoundRobin(struct Process p[], int n, int quantum)
{
    int time = 0;
    int completed = 0;

    float total_tat = 0;
    float total_wt = 0;

    for (int i = 0; i < n; i++)
        p[i].remaining = p[i].bt;

    printf("\n========== ROUND ROBIN ==========\n");

    while (completed < n)
    {
        int executed = 0;

        for (int i = 0; i < n; i++)
        {
            if (p[i].remaining > 0 && p[i].at <= time)
            {
                executed = 1;

                if (p[i].remaining > quantum)
                {
                    time = time + quantum;
                    p[i].remaining -= quantum;
                }
                else
                {
                    time = time + p[i].remaining;
                    p[i].remaining = 0;

                    p[i].ct = time;
                    p[i].tat = p[i].ct - p[i].at;
                    p[i].wt = p[i].tat - p[i].bt;

                    total_tat += p[i].tat;
                    total_wt += p[i].wt;

                    completed++;
                }
            }
        }

        if (executed == 0)
            time++;
    }

    printf("\nPID\tAT\tBT\tCT\tTAT\tWT\n");

    for (int i = 0; i < n; i++)
    {
        printf("P%d\t%d\t%d\t%d\t%d\t%d\n",
               p[i].pid,
               p[i].at,
               p[i].bt,
               p[i].ct,
               p[i].tat,
               p[i].wt);
    }

    printf("\nAverage TAT = %.2f", total_tat / n);
    printf("\nAverage WT  = %.2f\n", total_wt / n);
}

/* ---------- Main Function ---------- */
int main()
{
    struct Process p[MAX], temp[MAX];

    int n;
    int quantum;

    printf("========================================\n");
    printf("       CPU SCHEDULING ALGORITHMS\n");
    printf("========================================\n");

    printf("\nEnter number of processes: ");
    scanf("%d", &n);

    for (int i = 0; i < n; i++)
    {
        p[i].pid = i + 1;

        printf("\nProcess P%d\n", i + 1);

        printf("Enter Arrival Time: ");
        scanf("%d", &p[i].at);

        printf("Enter Burst Time: ");
        scanf("%d", &p[i].bt);

        printf("Enter Priority: ");
        scanf("%d", &p[i].priority);
    }

    printf("\nEnter Time Quantum for Round Robin: ");
    scanf("%d", &quantum);

    /* FCFS - sort by Arrival Time */
    for (int i = 0; i < n; i++)
        temp[i] = p[i];

    for (int i = 0; i < n - 1; i++)
    {
        for (int j = i + 1; j < n; j++)
        {
            if (temp[i].at > temp[j].at)
            {
                struct Process t = temp[i];
                temp[i] = temp[j];
                temp[j] = t;
            }
        }
    }

    FCFS(temp, n);

    /* SJF */
    for (int i = 0; i < n; i++)
        temp[i] = p[i];

    SJF(temp, n);

    /* Priority */
    for (int i = 0; i < n; i++)
        temp[i] = p[i];

    Priority(temp, n);

    /* Round Robin */
    for (int i = 0; i < n; i++)
        temp[i] = p[i];

    RoundRobin(temp, n, quantum);

    return 0;
}
