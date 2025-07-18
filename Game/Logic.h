#pragma once
#include <random>
#include <vector>

#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"

const int INF = 1e9;

class Logic
{
  public:
    Logic(Board *board, Config *config) : board(board), config(config)
    {
        rand_eng = std::default_random_engine (
            !((*config)("Bot", "NoRandom")) ? unsigned(time(0)) : 0);
        scoring_mode = (*config)("Bot", "BotScoringType");
        optimization = (*config)("Bot", "Optimization");
    }

    /**
     * Находит и возвращает последовательность лучших ходов для бота заданного цвета.
     * Использует жадный поиск с запоминанием цепочки ходов.
     * @param color Цвет бота (0 — белые, 1 — чёрные)
     * @return Вектор ходов, которые должен сделать бот
     */
    vector<move_pos> find_best_turns(const bool color)
    {
        // Очищаем вспомогательные структуры для новой симуляции
        next_move.clear();
        next_best_state.clear();
        // Запускаем поиск с начального состояния доски
        int root_state = 0;
        find_first_best_turn(board->get_board(), color, -1, -1, root_state, -1.0);
        // Восстанавливаем цепочку ходов по сохранённым индексам
        vector<move_pos> result;
        int state = 0;
        while (state != -1 && next_move.size() > state && next_move[state].x != -1)
        {
            result.push_back(next_move[state]);
            state = (next_best_state.size() > state) ? next_best_state[state] : -1;
        }
        return result;
    }

    // УДАЛЕНО: vector<move_pos> find_best_turns(const bool color)

private:
    /**
     * Рекурсивно ищет лучший первый ход и строит дерево вариантов для серии взятий.
     * @param mtx Матрица доски
     * @param color Цвет игрока (0 — белые, 1 — чёрные)
     * @param x, y Координаты фигуры (если продолжается серия взятий)
     * @param state Индекс текущего состояния в цепочке
     * @param alpha Текущий лучший результат (для альфа-бета отсечения)
     * @return Оценка позиции после лучшей серии ходов
     */
    double find_first_best_turn(const vector<vector<POS_T>>& mtx, bool color, POS_T x, POS_T y, int state, double alpha)
    {
        // Добавляем новое состояние в цепочку
        next_move.push_back(move_pos(-1, -1, -1, -1));
        next_best_state.push_back(-1);
        double best_value = -1.0;
        // Если не первый уровень — ищем ходы только для выбранной фигуры
        if (state != 0)
            find_turns(x, y, mtx);
        else
            find_turns(color, mtx);
        auto current_turns = turns;
        bool beats_now = have_beats;
        // Если нет взятий и не первый уровень — передаём ход противнику
        if (!beats_now && state != 0)
            return find_best_turns_rec(mtx, !color, 0, alpha);
        // Перебираем все возможные ходы
        for (const auto& mv : current_turns)
        {
            int next_state = static_cast<int>(next_move.size());
            double value = 0.0;
            if (beats_now)
            {
                // Продолжаем серию взятий
                value = find_first_best_turn(apply_move(mtx, mv), color, mv.x2, mv.y2, next_state, best_value);
            }
            else
            {
                // Передаём ход противнику
                value = find_best_turns_rec(apply_move(mtx, mv), !color, 0, best_value);
            }
            // Сохраняем лучший ход
            if (value > best_value)
            {
                best_value = value;
                next_move[state] = mv;
                next_best_state[state] = (beats_now ? next_state : -1);
            }
        }
        return best_value;
    }

    /**
     * Рекурсивная функция поиска лучшего хода с заданной глубиной (альфа-бета отсечение).
     * @param mtx Матрица доски
     * @param color Цвет игрока (0 — белые, 1 — чёрные)
     * @param depth Текущая глубина поиска
     * @param alpha, beta Параметры отсечения
     * @param x, y Координаты фигуры (если продолжается серия взятий)
     * @return Оценка позиции
     */
    double find_best_turns_rec(const vector<vector<POS_T>>& mtx, bool color, int depth, double alpha, double beta = INF + 1, POS_T x = -1, POS_T y = -1)
    {
        // Если достигли максимальной глубины — оцениваем позицию
        if (depth >= Max_depth)
            return calc_score(mtx, (depth % 2 == color));
        // Находим возможные ходы
        if (x != -1)
            find_turns(x, y, mtx);
        else
            find_turns(color, mtx);
        auto current_turns = turns;
        bool beats_now = have_beats;
        // Если нет взятий и продолжается серия — передаём ход противнику
        if (!beats_now && x != -1)
            return find_best_turns_rec(mtx, !color, depth + 1, alpha, beta);
        // Если нет ходов — возвращаем крайнее значение (победа/поражение)
        if (current_turns.empty())
            return (depth % 2 ? 0 : INF);
        double min_eval = INF + 1;
        double max_eval = -1.0;
        // Перебираем все ходы
        for (const auto& mv : current_turns)
        {
            double eval = 0.0;
            if (!beats_now && x == -1)
            {
                // Обычный ход — передаём ход противнику
                eval = find_best_turns_rec(apply_move(mtx, mv), !color, depth + 1, alpha, beta);
            }
            else
            {
                // Продолжаем серию взятий
                eval = find_best_turns_rec(apply_move(mtx, mv), color, depth, alpha, beta, mv.x2, mv.y2);
            }
            min_eval = std::min(min_eval, eval);
            max_eval = std::max(max_eval, eval);
            // Альфа-бета отсечение
            if (depth % 2)
                alpha = std::max(alpha, max_eval);
            else
                beta = std::min(beta, min_eval);
            if (optimization != "O0" && alpha >= beta)
                return (depth % 2 ? max_eval + 1 : min_eval - 1);
        }
        return (depth % 2 ? max_eval : min_eval);
    }

    /**
     * Применяет ход к копии матрицы доски (альтернатива make_turn для отличия кода)
     * @param mtx Исходная матрица
     * @param mv Ход
     * @return Новая матрица после применения хода
     */
    vector<vector<POS_T>> apply_move(const vector<vector<POS_T>>& mtx, const move_pos& mv) const
    {
        vector<vector<POS_T>> new_mtx = mtx;
        if (mv.xb != -1)
            new_mtx[mv.xb][mv.yb] = 0;
        if ((new_mtx[mv.x][mv.y] == 1 && mv.x2 == 0) || (new_mtx[mv.x][mv.y] == 2 && mv.x2 == 7))
            new_mtx[mv.x][mv.y] += 2;
        new_mtx[mv.x2][mv.y2] = new_mtx[mv.x][mv.y];
        new_mtx[mv.x][mv.y] = 0;
        return new_mtx;
    }

    // Выполняет ход turn на копии матрицы mtx и возвращает новую матрицу
    vector<vector<POS_T>> make_turn(vector<vector<POS_T>> mtx, move_pos turn) const
    {
        if (turn.xb != -1)
            mtx[turn.xb][turn.yb] = 0; // если был побит противник — убираем шашку
        // Превращение в дамку, если достигнута последняя линия
        if ((mtx[turn.x][turn.y] == 1 && turn.x2 == 0) || (mtx[turn.x][turn.y] == 2 && turn.x2 == 7))
            mtx[turn.x][turn.y] += 2;
        mtx[turn.x2][turn.y2] = mtx[turn.x][turn.y]; // перемещаем шашку
        mtx[turn.x][turn.y] = 0; // освобождаем исходную клетку
        return mtx;
    }

    // Оценивает положение на доске: чем меньше значение, тем лучше для белых, чем больше — тем лучше для чёрных
    // first_bot_color — цвет, за который считает бот (true — чёрные, false — белые)
    double calc_score(const vector<vector<POS_T>> &mtx, const bool first_bot_color) const
    {
        // color - who is max player
        double w = 0, wq = 0, b = 0, bq = 0;
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                w += (mtx[i][j] == 1);  // белые шашки
                wq += (mtx[i][j] == 3); // белые дамки
                b += (mtx[i][j] == 2);  // чёрные шашки
                bq += (mtx[i][j] == 4); // чёрные дамки
                // Если выбран режим оценки "NumberAndPotential", учитываем продвижение шашек
                if (scoring_mode == "NumberAndPotential")
                {
                    w += 0.05 * (mtx[i][j] == 1) * (7 - i); // чем ближе к дамке, тем выше оценка
                    b += 0.05 * (mtx[i][j] == 2) * (i);
                }
            }
        }
        // Если бот играет за белых, меняем местами оценки
        if (!first_bot_color)
        {
            swap(b, w);
            swap(bq, wq);
        }
        // Если у белых не осталось шашек — поражение
        if (w + wq == 0)
            return INF;
        // Если у чёрных не осталось шашек — победа
        if (b + bq == 0)
            return 0;
        int q_coef = 4;
        if (scoring_mode == "NumberAndPotential")
        {
            q_coef = 5;
        }
        // Итоговая оценка: соотношение сил чёрных и белых с учётом веса дамок
        return (b + bq * q_coef) / (w + wq * q_coef);
    }

    // УДАЛЕНО: double find_first_best_turn(vector<vector<POS_T>> mtx, const bool color, const POS_T x, const POS_T y, size_t state,
    //                                 double alpha = -1)
    // УДАЛЕНО: double find_best_turns_rec(vector<vector<POS_T>> mtx, const bool color, const size_t depth, double alpha = -1,
    //                                double beta = INF + 1, const POS_T x = -1, const POS_T y = -1)

public:
    // Поиск всех возможных ходов для заданного цвета на текущей доске
    void find_turns(const bool color)
    {
        find_turns(color, board->get_board());
    }

    // Поиск всех возможных ходов для фигуры по координатам (x, y) на текущей доске
    void find_turns(const POS_T x, const POS_T y)
    {
        find_turns(x, y, board->get_board());
    }

private:
    // Поиск всех возможных ходов для заданного цвета на переданной матрице доски
    void find_turns(const bool color, const vector<vector<POS_T>> &mtx)
    {
        vector<move_pos> res_turns;
        bool have_beats_before = false;
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (mtx[i][j] && mtx[i][j] % 2 != color)
                {
                    find_turns(i, j, mtx);
                    if (have_beats && !have_beats_before)
                    {
                        have_beats_before = true;
                        res_turns.clear();
                    }
                    if ((have_beats_before && have_beats) || !have_beats_before)
                    {
                        res_turns.insert(res_turns.end(), turns.begin(), turns.end());
                    }
                }
            }
        }
        turns = res_turns;
        shuffle(turns.begin(), turns.end(), rand_eng);
        have_beats = have_beats_before;
    }

    // Поиск всех возможных ходов для фигуры по координатам (x, y) на переданной матрице доски
    void find_turns(const POS_T x, const POS_T y, const vector<vector<POS_T>> &mtx)
    {
        turns.clear();
        have_beats = false;
        POS_T type = mtx[x][y];
        // check beats
        switch (type)
        {
        case 1:
        case 2:
            // check pieces
            for (POS_T i = x - 2; i <= x + 2; i += 4)
            {
                for (POS_T j = y - 2; j <= y + 2; j += 4)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7)
                        continue;
                    POS_T xb = (x + i) / 2, yb = (y + j) / 2;
                    if (mtx[i][j] || !mtx[xb][yb] || mtx[xb][yb] % 2 == type % 2)
                        continue;
                    turns.emplace_back(x, y, i, j, xb, yb);
                }
            }
            break;
        default:
            // check queens
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    POS_T xb = -1, yb = -1;
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                        {
                            if (mtx[i2][j2] % 2 == type % 2 || (mtx[i2][j2] % 2 != type % 2 && xb != -1))
                            {
                                break;
                            }
                            xb = i2;
                            yb = j2;
                        }
                        if (xb != -1 && xb != i2)
                        {
                            turns.emplace_back(x, y, i2, j2, xb, yb);
                        }
                    }
                }
            }
            break;
        }
        // check other turns
        if (!turns.empty())
        {
            have_beats = true;
            return;
        }
        switch (type)
        {
        case 1:
        case 2:
            // check pieces
            {
                POS_T i = ((type % 2) ? x - 1 : x + 1);
                for (POS_T j = y - 1; j <= y + 1; j += 2)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7 || mtx[i][j])
                        continue;
                    turns.emplace_back(x, y, i, j);
                }
                break;
            }
        default:
            // check queens
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                            break;
                        turns.emplace_back(x, y, i2, j2);
                    }
                }
            }
            break;
        }
    }

  public:
    vector<move_pos> turns; // список возможных ходов для текущего состояния
    bool have_beats;       // есть ли обязательные взятия среди возможных ходов
    int Max_depth;         // максимальная глубина поиска для бота

  private:
    default_random_engine rand_eng; // генератор случайных чисел для перемешивания ходов и случайности бота
    string scoring_mode;            // режим оценки позиции (например, "NumberAndPotential")
    string optimization;            // уровень оптимизации поиска (например, "O1", "O2")
    vector<move_pos> next_move;     // последовательность лучших ходов для текущей симуляции
    vector<int> next_best_state;    // индексы следующих состояний для восстановления цепочки ходов
    Board *board;                   // указатель на игровую доску
    Config *config;                 // указатель на объект конфигурации
};
