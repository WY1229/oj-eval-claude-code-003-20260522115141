#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

using namespace std;

struct FreezeSub {
    int status; // 0=Accepted, 1=Wrong_Answer, 2=Runtime_Error, 3=Time_Limit_Exceed
    int time;
};

struct ProblemInfo {
    bool solved = false;
    int solve_time = 0;
    int wrong_before_solve = 0;
    int wrong_after_solve = 0;
    int wrong_total = 0;

    bool frozen = false;
    int wrong_before_freeze = 0;
    int submit_after_freeze = 0;
    vector<FreezeSub> freeze_subs;
};

struct SubmissionRecord {
    char prob_char;
    int status; // 0=Accepted, 1=Wrong_Answer, 2=Runtime_Error, 3=Time_Limit_Exceed
    int time;
};

struct Team {
    string name;
    array<ProblemInfo, 26> problems;
    int solved_count = 0;
    int penalty_time = 0;
    vector<int> solve_times;
    vector<SubmissionRecord> all_subs;
};

inline int parse_status(const string& s) {
    if (s.size() == 8) return 0; // Accepted
    if (s[0] == 'W') return 1;   // Wrong_Answer
    if (s[0] == 'R') return 2;   // Runtime_Error
    return 3;                     // Time_Limit_Exceed
}

inline bool ranks_higher(const Team& a, const Team& b) {
    if (a.solved_count != b.solved_count)
        return a.solved_count > b.solved_count;
    if (a.penalty_time != b.penalty_time)
        return a.penalty_time < b.penalty_time;
    int n = (int)a.solve_times.size();
    for (int i = n - 1; i >= 0; i--) {
        if (a.solve_times[i] != b.solve_times[i])
            return a.solve_times[i] < b.solve_times[i];
    }
    return a.name < b.name;
}

struct OutputBuffer {
    static const int BUF_SIZE = 1 << 20;
    char buf[BUF_SIZE];
    int pos = 0;

    void put(const char* s) {
        int len = 0;
        const char* p = s;
        while (*p) { len++; p++; }
        if (pos + len <= BUF_SIZE) {
            memcpy(buf + pos, s, len);
            pos += len;
        } else {
            if (pos > 0) { fwrite(buf, 1, pos, stdout); pos = 0; }
            if (len <= BUF_SIZE) {
                memcpy(buf, s, len);
                pos = len;
            } else {
                fwrite(s, 1, len, stdout);
            }
        }
    }

    void put(char c) {
        if (pos == BUF_SIZE) { fwrite(buf, 1, pos, stdout); pos = 0; }
        buf[pos++] = c;
    }

    void puts(const char* s, int len) {
        if (pos + len <= BUF_SIZE) {
            memcpy(buf + pos, s, len);
            pos += len;
        } else {
            if (pos > 0) { fwrite(buf, 1, pos, stdout); pos = 0; }
            if (len <= BUF_SIZE) {
                memcpy(buf, s, len);
                pos = len;
            } else {
                fwrite(s, 1, len, stdout);
            }
        }
    }

    void put_int(int x) {
        if (x == 0) { put('0'); return; }
        char tmp[16];
        int len = 0;
        while (x) { tmp[len++] = '0' + (x % 10); x /= 10; }
        for (int i = len - 1; i >= 0; i--) put(tmp[i]);
    }

    void info(const char* msg) { put("[Info]"); put(msg); put('\n'); }
    void error(const char* msg) { put("[Error]"); put(msg); put('\n'); }

    void flush() { if (pos > 0) { fwrite(buf, 1, pos, stdout); pos = 0; } }
};

OutputBuffer ob;

void output_scoreboard(const vector<int>& ranking, const vector<Team>& teams, int pc) {
    char line[4096];
    for (size_t ri = 0; ri < ranking.size(); ri++) {
        const Team& t = teams[ranking[ri]];
        int p = 0;
        // Copy team name
        for (char c : t.name) line[p++] = c;
        line[p++] = ' ';
        p += sprintf(line + p, "%d %d %d", (int)ri + 1, t.solved_count, t.penalty_time);
        for (int pi = 0; pi < pc; pi++) {
            const ProblemInfo& pr = t.problems[pi];
            if (!pr.frozen && !pr.solved && pr.wrong_total == 0) {
                line[p++] = ' '; line[p++] = '.';
            } else if (pr.frozen) {
                if (pr.wrong_before_freeze == 0)
                    p += sprintf(line + p, " 0/%d", pr.submit_after_freeze);
                else
                    p += sprintf(line + p, " -%d/%d", pr.wrong_before_freeze, pr.submit_after_freeze);
            } else if (pr.solved) {
                if (pr.wrong_before_solve == 0)
                    { line[p++] = ' '; line[p++] = '+'; }
                else
                    p += sprintf(line + p, " +%d", pr.wrong_before_solve);
            } else {
                p += sprintf(line + p, " -%d", pr.wrong_total);
            }
        }
        line[p++] = '\n';
        ob.puts(line, p);
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // Read all stdin at once
    string input;
    input.reserve(20 * 1024 * 1024);
    char buf[1 << 18];
    size_t nr;
    while ((nr = fread(buf, 1, sizeof(buf), stdin)) > 0)
        input.append(buf, nr);

    vector<Team> teams;
    unordered_map<string, int> team_name_to_id;
    vector<int> ranking;
    bool started = false;
    bool frozen_state = false;
    bool ever_flushed = false;
    bool ranking_dirty = false;
    int problem_count = 0;

    size_t ipos = 0;
    const size_t ilen = input.size();

    while (ipos < ilen) {
        size_t start = ipos;
        while (ipos < ilen && input[ipos] != '\n') ipos++;
        string line(input, start, ipos - start);
        if (ipos < ilen) ipos++;

        if (line.empty()) continue;

        char c0 = line[0];

        if (c0 == 'A') {
            // ADDTEAM
            string name(line, 8); // "ADDTEAM " = 8 chars
            if (started) {
                ob.error("Add failed: competition has started.");
            } else if (team_name_to_id.count(name)) {
                ob.error("Add failed: duplicated team name.");
            } else {
                int id = (int)teams.size();
                team_name_to_id[name] = id;
                Team t;
                t.name = name;
                teams.push_back(t);
                ob.info("Add successfully.");
            }
        }
        else if (c0 == 'S' && line[1] == 'T') {
            // START
            size_t p1 = line.find("DURATION ") + 9;
            size_t p2 = line.find(" PROBLEM ", p1);
            int dur = stoi(line.substr(p1, p2 - p1));
            (void)dur;
            int pc = stoi(line.substr(p2 + 9));
            if (started) {
                ob.error("Start failed: competition has started.");
            } else {
                started = true;
                problem_count = pc;
                ob.info("Competition starts.");
            }
        }
        else if (c0 == 'S' && line[1] == 'U') {
            // SUBMIT
            // SUBMIT [prob] BY [team] WITH [status] AT [time]
            size_t p1 = 7;
            size_t p2 = line.find(" BY ", p1);
            size_t p3 = line.find(" WITH ", p2);
            size_t p4 = line.find(" AT ", p3);

            char prob_char = line[p1];
            int prob_idx = prob_char - 'A';
            string team_name(line, p2 + 4, p3 - p2 - 4);
            string status_str(line, p3 + 6, p4 - p3 - 6);
            int time = stoi(line.substr(p4 + 4));
            int status_id = parse_status(status_str);

            int tid = team_name_to_id[team_name];
            Team& t = teams[tid];
            t.all_subs.push_back({prob_char, status_id, time});

            ProblemInfo& p = t.problems[prob_idx];

            if (p.solved) {
                if (status_id != 0) p.wrong_after_solve++;
            } else if (frozen_state) {
                if (!p.frozen) {
                    p.frozen = true;
                    // wrong_before_freeze was captured at FREEZE time (or is 0 for new problems)
                }
                p.freeze_subs.push_back({status_id, time});
                p.submit_after_freeze++;
            } else if (status_id == 0) {
                p.solved = true;
                p.solve_time = time;
                int penalty = 20 * p.wrong_before_solve + time;
                t.penalty_time += penalty;
                t.solved_count++;
                t.solve_times.push_back(time);
                ranking_dirty = true;
            } else {
                p.wrong_total++;
                p.wrong_before_solve++;
            }
        }
        else if (c0 == 'F' && line[1] == 'L') {
            // FLUSH
            if (!ever_flushed) ever_flushed = true;
            ranking.clear();
            ranking.reserve(teams.size());
            for (int i = 0; i < (int)teams.size(); i++)
                ranking.push_back(i);
            sort(ranking.begin(), ranking.end(), [&](int a, int b) {
                return ranks_higher(teams[a], teams[b]);
            });
            ob.info("Flush scoreboard.");
        }
        else if (c0 == 'F' && line[1] == 'R') {
            // FREEZE
            if (frozen_state) {
                ob.error("Freeze failed: scoreboard has been frozen.");
            } else {
                frozen_state = true;
                for (auto& t : teams) {
                    for (int p = 0; p < problem_count; p++) {
                        auto& pi = t.problems[p];
                        if (!pi.solved) {
                            pi.wrong_before_freeze = pi.wrong_total;
                        }
                    }
                }
                ob.info("Freeze scoreboard.");
            }
        }
        else if (c0 == 'S' && line[1] == 'C') {
            // SCROLL
            if (!frozen_state) {
                ob.error("Scroll failed: scoreboard has not been frozen.");
            } else {
                frozen_state = false;

                // Flush
                ranking.clear();
                ranking.reserve(teams.size());
                for (int i = 0; i < (int)teams.size(); i++)
                    ranking.push_back(i);
                sort(ranking.begin(), ranking.end(), [&](int a, int b) {
                    return ranks_higher(teams[a], teams[b]);
                });
                ever_flushed = true;

                ob.put("[Info]Scroll scoreboard.\n");
                output_scoreboard(ranking, teams, problem_count);

                int n = (int)ranking.size();
                int bottom = n - 1;

                while (bottom >= 0) {
                    int cur_id = ranking[bottom];
                    Team& cur = teams[cur_id];

                    // Find smallest frozen problem
                    int sfp = -1;
                    for (int p = 0; p < problem_count; p++) {
                        if (cur.problems[p].frozen) {
                            sfp = p;
                            break;
                        }
                    }
                    if (sfp == -1) { bottom--; continue; }

                    ProblemInfo& pinfo = cur.problems[sfp];
                    int old_solved = cur.solved_count;
                    int old_penalty = cur.penalty_time;

                    int first_ac = -1;
                    for (int i = 0; i < (int)pinfo.freeze_subs.size(); i++) {
                        if (pinfo.freeze_subs[i].status == 0) {
                            first_ac = i;
                            break;
                        }
                    }

                    if (first_ac >= 0) {
                        int wrong_before_ac = 0;
                        for (int i = 0; i < first_ac; i++) {
                            if (pinfo.freeze_subs[i].status != 0) wrong_before_ac++;
                        }
                        pinfo.solved = true;
                        pinfo.solve_time = pinfo.freeze_subs[first_ac].time;
                        pinfo.wrong_before_solve = pinfo.wrong_before_freeze + wrong_before_ac;
                        int penalty = 20 * pinfo.wrong_before_solve + pinfo.solve_time;
                        cur.penalty_time += penalty;
                        cur.solved_count++;
                        cur.solve_times.push_back(pinfo.solve_time);
                        for (int i = first_ac + 1; i < (int)pinfo.freeze_subs.size(); i++) {
                            if (pinfo.freeze_subs[i].status != 0) pinfo.wrong_after_solve++;
                        }
                    } else {
                        int fw = 0;
                        for (auto& fs : pinfo.freeze_subs) {
                            if (fs.status != 0) fw++;
                        }
                        pinfo.wrong_total += fw;
                        pinfo.wrong_before_solve += fw;
                    }

                    pinfo.frozen = false;
                    pinfo.freeze_subs.clear();
                    pinfo.submit_after_freeze = 0;

                    if (cur.solved_count == old_solved && cur.penalty_time == old_penalty)
                        continue;

                    // Binary search for new position
                    int old_pos = bottom;
                    int lo = 0, hi = old_pos;
                    while (lo < hi) {
                        int mid = (lo + hi) / 2;
                        if (ranks_higher(cur, teams[ranking[mid]]))
                            hi = mid;
                        else
                            lo = mid + 1;
                    }
                    int new_pos = lo;

                    if (new_pos < old_pos) {
                        int moving = ranking[old_pos];
                        for (int i = old_pos; i > new_pos; i--)
                            ranking[i] = ranking[i - 1];
                        ranking[new_pos] = moving;

                        ob.put(cur.name.c_str());
                        ob.put(' ');
                        ob.put(teams[ranking[new_pos + 1]].name.c_str());
                        ob.put(' ');
                        ob.put_int(cur.solved_count);
                        ob.put(' ');
                        ob.put_int(cur.penalty_time);
                        ob.put('\n');
                    }
                }

                output_scoreboard(ranking, teams, problem_count);
            }
        }
        else if (c0 == 'Q' && line[6] == 'R') {
            // QUERY_RANKING
            string name(line, 14); // "QUERY_RANKING " = 14 chars
            auto it = team_name_to_id.find(name);
            if (it == team_name_to_id.end()) {
                ob.error("Query ranking failed: cannot find the team.");
            } else {
                ob.info("Complete query ranking.");
                if (frozen_state) {
                    ob.put("[Warning]Scoreboard is frozen. The ranking may be inaccurate until it were scrolled.\n");
                }
                if (ranking.empty() && !ever_flushed) {
                    ranking.clear();
                    ranking.reserve(teams.size());
                    for (int i = 0; i < (int)teams.size(); i++)
                        ranking.push_back(i);
                    sort(ranking.begin(), ranking.end(), [&](int a, int b) {
                        return teams[a].name < teams[b].name;
                    });
                }
                int pos = 1;
                for (size_t i = 0; i < ranking.size(); i++) {
                    if (ranking[i] == it->second) { pos = (int)i + 1; break; }
                }
                ob.put(name.c_str());
                ob.put(" NOW AT RANKING ");
                ob.put_int(pos);
                ob.put('\n');
            }
        }
        else if (c0 == 'Q' && line[6] == 'S') {
            // QUERY_SUBMISSION
            // QUERY_SUBMISSION [team_name] WHERE PROBLEM=[X] AND STATUS=[Y]
            size_t p1 = 17; // "QUERY_SUBMISSION " = 17 chars
            size_t p2 = line.find(" WHERE PROBLEM=", p1);
            string team_name(line, p1, p2 - p1);
            size_t p_eq1 = p2 + 15; // " WHERE PROBLEM=" = 15 chars
            size_t p_and = line.find(" AND STATUS=", p_eq1);
            string prob_val(line, p_eq1, p_and - p_eq1);
            size_t p_eq2 = p_and + 12; // " AND STATUS=" = 12 chars
            string status_val(line, p_eq2);

            auto it = team_name_to_id.find(team_name);
            if (it == team_name_to_id.end()) {
                ob.error("Query submission failed: cannot find the team.");
            } else {
                const Team& t = teams[it->second];
                ob.info("Complete query submission.");

                int found_idx = -1;
                for (int i = (int)t.all_subs.size() - 1; i >= 0; i--) {
                    const auto& rec = t.all_subs[i];
                    bool prob_ok = (prob_val == "ALL" || prob_val.size() == 1 && prob_val[0] == rec.prob_char);
                    bool status_ok = (status_val == "ALL");
                    if (!status_ok) {
                        int want_status = parse_status(status_val);
                        status_ok = (want_status == rec.status);
                    }
                    if (prob_ok && status_ok) {
                        found_idx = i;
                        break;
                    }
                }

                if (found_idx == -1) {
                    ob.put("Cannot find any submission.\n");
                } else {
                    const auto& rec = t.all_subs[found_idx];
                    static const char* status_names[] = {"Accepted", "Wrong_Answer", "Runtime_Error", "Time_Limit_Exceed"};
                    ob.put(team_name.c_str());
                    ob.put(' ');
                    ob.put(rec.prob_char);
                    ob.put(' ');
                    ob.put(status_names[rec.status]);
                    ob.put(' ');
                    ob.put_int(rec.time);
                    ob.put('\n');
                }
            }
        }
        else if (c0 == 'E') {
            ob.info("Competition ends.");
            break;
        }
    }

    ob.flush();
    return 0;
}
