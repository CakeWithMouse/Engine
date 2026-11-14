#include "GraphLayout.h"

#include <QSet>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <limits>

namespace EditorCore
{
namespace
{
constexpr qreal LayerGap = 80.0;  // между столбцами слоёв
constexpr qreal RowGap = 40.0;    // между нодами в слое
constexpr qreal SiblingGap = 40.0; // между поддеревьями
constexpr qreal DepthGap = 60.0;  // между уровнями дерева

struct Graph
{
    QVector<int> ids;            // в порядке «слева направо, сверху вниз» по текущим позициям
    QHash<int, int> index;       // id -> индекс в ids
    QVector<QVector<int>> succ;  // индексы; без петель и повторов
    QVector<QVector<int>> pred;
    QVector<QSizeF> size;
    QVector<QPointF> pos;
};

Graph Build(const GraphDocument& document, const GraphNodeSize& size)
{
    Graph g;
    QVector<const GraphNodeData*> nodes;
    for (const auto& node : document.Nodes()) nodes.push_back(&node);
    std::stable_sort(nodes.begin(), nodes.end(), [](const GraphNodeData* a, const GraphNodeData* b) {
        return a->pos.x() != b->pos.x() ? a->pos.x() < b->pos.x() : a->pos.y() < b->pos.y();
    });
    for (const auto* node : nodes)
    {
        g.index.insert(node->id, g.ids.size());
        g.ids.push_back(node->id);
        g.size.push_back(size ? size(*node) : QSizeF(150, 80));
        g.pos.push_back(node->pos);
    }
    g.succ.resize(g.ids.size());
    g.pred.resize(g.ids.size());
    for (const auto& c : document.Connections())
    {
        const int from = g.index.value(c.fromNode, -1), to = g.index.value(c.toNode, -1);
        if (from < 0 || to < 0 || from == to || g.succ[from].contains(to)) continue;
        g.succ[from].push_back(to);
        g.pred[to].push_back(from);
    }
    return g;
}

QPointF Origin(const Graph& g)
{
    qreal x = std::numeric_limits<qreal>::max(), y = std::numeric_limits<qreal>::max();
    for (const auto& p : g.pos)
    {
        x = std::min(x, p.x());
        y = std::min(y, p.y());
    }
    return g.pos.isEmpty() ? QPointF() : QPointF(x, y);
}

// Сдвиг к прежнему левому верхнему углу и привязка к сетке.
QHash<int, QPointF> Finish(const Graph& g, const QVector<QPointF>& placed, qreal grid)
{
    QHash<int, QPointF> result;
    if (placed.isEmpty()) return result;
    qreal minX = std::numeric_limits<qreal>::max(), minY = std::numeric_limits<qreal>::max();
    for (const auto& p : placed)
    {
        minX = std::min(minX, p.x());
        minY = std::min(minY, p.y());
    }
    const QPointF shift = Origin(g) - QPointF(minX, minY);
    auto snap = [grid](qreal v) { return grid > 0 ? std::round(v / grid) * grid : v; };
    for (int i = 0; i < g.ids.size(); ++i)
        result.insert(g.ids[i], QPointF(snap(placed[i].x() + shift.x()), snap(placed[i].y() + shift.y())));
    return result;
}

QHash<int, QPointF> Layered(Graph g, qreal grid)
{
    const int n = g.ids.size();
    // Обратные связи циклов (поток кода может замыкаться) выбрасываются. Порядок нод — эвристика Идса:
    // стоки уходят в конец, источники в начало, а из оставшегося первой берётся нода с наибольшим
    // перевесом выходов над входами. Обратная связь — ведущая к ноде раньше по этому порядку.
    QVector<int> outDegree(n), inDegree(n);
    for (int i = 0; i < n; ++i)
    {
        outDegree[i] = g.succ[i].size();
        inDegree[i] = g.pred[i].size();
    }
    QVector<bool> removed(n, false);
    QVector<int> head, tail;
    auto remove = [&](int node) {
        removed[node] = true;
        for (const int to : g.succ[node]) if (!removed[to]) --inDegree[to];
        for (const int from : g.pred[node]) if (!removed[from]) --outDegree[from];
    };
    for (int left = n; left > 0;)
    {
        bool progress = true;
        while (progress)
        {
            progress = false;
            for (int i = 0; i < n; ++i)
                if (!removed[i] && outDegree[i] == 0 && inDegree[i] > 0) { tail.prepend(i); remove(i); --left; progress = true; }
            for (int i = 0; i < n; ++i)
                if (!removed[i] && inDegree[i] == 0) { head.push_back(i); remove(i); --left; progress = true; }
        }
        int best = -1;
        for (int i = 0; i < n; ++i)
            if (!removed[i] && (best < 0 || outDegree[i] - inDegree[i] > outDegree[best] - inDegree[best])) best = i;
        if (best < 0) break;
        head.push_back(best);
        remove(best);
        --left;
    }
    QVector<int> rankOf(n);
    const QVector<int> sequence = head + tail;
    for (int i = 0; i < sequence.size(); ++i) rankOf[sequence[i]] = i;
    QVector<QVector<int>> dag(n);
    for (int i = 0; i < n; ++i)
        for (const int to : g.succ[i])
            if (rankOf[i] < rankOf[to]) dag[i].push_back(to);
    QVector<QVector<int>> dagPred(n);
    for (int i = 0; i < n; ++i)
        for (const int to : dag[i]) dagPred[to].push_back(i);

    // Топологический порядок; слой — длина самого длинного пути от источника.
    QVector<int> incoming(n), topo;
    for (int i = 0; i < n; ++i) incoming[i] = dagPred[i].size();
    for (int i = 0; i < n; ++i)
        if (!incoming[i]) topo.push_back(i);
    for (int k = 0; k < topo.size(); ++k)
        for (const int to : dag[topo[k]])
            if (--incoming[to] == 0) topo.push_back(to);
    QVector<int> layer(n, 0);
    for (const int node : topo)
        for (const int to : dag[node]) layer[to] = std::max(layer[to], layer[node] + 1);
    // Источник с потребителями встаёт прямо перед ближайшим из них, а не в первый столбец.
    for (int k = topo.size() - 1; k >= 0; --k)
    {
        const int node = topo[k];
        if (!dagPred[node].isEmpty() || dag[node].isEmpty()) continue;
        int nearest = std::numeric_limits<int>::max();
        for (const int to : dag[node]) nearest = std::min(nearest, layer[to]);
        layer[node] = std::max(0, nearest - 1);
    }

    int layers = 0;
    for (const int l : layer) layers = std::max(layers, l + 1);
    QVector<QVector<int>> rows(layers);
    QVector<int> order(n); // по текущему Y, затем X
    for (int i = 0; i < n; ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        return g.pos[a].y() != g.pos[b].y() ? g.pos[a].y() < g.pos[b].y() : g.pos[a].x() < g.pos[b].x();
    });
    for (const int node : order) rows[layer[node]].push_back(node);

    // Меньше пересечений: несколько проходов «по средним положениям соседей».
    QVector<qreal> rank(n);
    auto updateRank = [&] {
        for (const auto& row : rows)
            for (int i = 0; i < row.size(); ++i) rank[row[i]] = i;
    };
    updateRank();
    for (int pass = 0; pass < 4; ++pass)
    {
        const bool down = pass % 2 == 0;
        for (int r = down ? 1 : layers - 2; down ? r < layers : r >= 0; r += down ? 1 : -1)
        {
            QVector<qreal> key(n);
            for (const int node : rows[r])
            {
                const auto& neighbours = down ? dagPred[node] : dag[node];
                qreal sum = 0;
                for (const int other : neighbours) sum += rank[other];
                key[node] = neighbours.isEmpty() ? rank[node] : sum / neighbours.size();
            }
            std::stable_sort(rows[r].begin(), rows[r].end(), [&](int a, int b) { return key[a] < key[b]; });
            for (int i = 0; i < rows[r].size(); ++i) rank[rows[r][i]] = i;
        }
    }

    // Координаты: столбцы по самой широкой ноде слоя; нода тянется к среднему Y своих предшественников.
    QVector<QPointF> placed(n);
    qreal x = 0;
    for (int r = 0; r < layers; ++r)
    {
        qreal width = 0, bottom = -RowGap;
        for (const int node : rows[r])
        {
            const QSizeF s = g.size[node];
            qreal y = bottom + RowGap;
            if (!dagPred[node].isEmpty())
            {
                qreal centre = 0;
                for (const int from : dagPred[node]) centre += placed[from].y() + g.size[from].height() / 2;
                y = std::max(y, centre / dagPred[node].size() - s.height() / 2);
            }
            placed[node] = QPointF(x, y);
            bottom = y + s.height();
            width = std::max(width, s.width());
        }
        x += width + LayerGap;
    }
    return Finish(g, placed, grid);
}

QHash<int, QPointF> Tree(const Graph& g, qreal grid)
{
    const int n = g.ids.size();
    // Дети в порядке их X: так дерево поведения задаёт очередь детей.
    QVector<QVector<int>> children(n);
    for (int i = 0; i < n; ++i)
    {
        children[i] = g.succ[i];
        std::stable_sort(children[i].begin(), children[i].end(), [&](int a, int b) {
            return g.pos[a].x() != g.pos[b].x() ? g.pos[a].x() < g.pos[b].x() : g.pos[a].y() < g.pos[b].y();
        });
    }
    // Остов: у каждой ноды один родитель — тот, кто дошёл до неё первым.
    QVector<int> roots;
    for (int i = 0; i < n; ++i)
        if (g.pred[i].isEmpty()) roots.push_back(i);
    QVector<bool> seen(n, false);
    QVector<QVector<int>> tree(n);
    QVector<int> depth(n, 0);
    auto claim = [&](int root) {
        seen[root] = true;
        QVector<int> queue{root};
        for (int k = 0; k < queue.size(); ++k)
            for (const int child : children[queue[k]])
                if (!seen[child])
                {
                    seen[child] = true;
                    depth[child] = depth[queue[k]] + 1;
                    tree[queue[k]].push_back(child);
                    queue.push_back(child);
                }
    };
    for (const int root : roots) claim(root);
    for (int i = 0; i < n; ++i) // ноды на циклах без корня
        if (!seen[i])
        {
            roots.push_back(i);
            claim(i);
        }

    int depths = 0;
    for (const int d : depth) depths = std::max(depths, d + 1);
    QVector<qreal> rowHeight(depths, 0), rowY(depths, 0);
    for (int i = 0; i < n; ++i) rowHeight[depth[i]] = std::max(rowHeight[depth[i]], g.size[i].height());
    for (int d = 1; d < depths; ++d) rowY[d] = rowY[d - 1] + rowHeight[d - 1] + DepthGap;

    // Ширина поддерева, затем размещение слева направо.
    QVector<qreal> span(n, 0);
    std::function<qreal(int)> measure = [&](int node) {
        qreal kids = 0;
        for (int i = 0; i < tree[node].size(); ++i) kids += measure(tree[node][i]) + (i ? SiblingGap : 0);
        return span[node] = std::max(g.size[node].width(), kids);
    };
    QVector<QPointF> placed(n);
    std::function<void(int, qreal)> place = [&](int node, qreal left) {
        qreal kids = 0;
        for (int i = 0; i < tree[node].size(); ++i) kids += span[tree[node][i]] + (i ? SiblingGap : 0);
        placed[node] = QPointF(left + (span[node] - g.size[node].width()) / 2, rowY[depth[node]]);
        qreal x = left + (span[node] - kids) / 2;
        for (const int child : tree[node])
        {
            place(child, x);
            x += span[child] + SiblingGap;
        }
    };
    qreal left = 0;
    for (const int root : roots)
    {
        measure(root);
        place(root, left);
        left += span[root] + SiblingGap;
    }
    return Finish(g, placed, grid);
}
} // namespace

QHash<int, QPointF> ArrangeGraph(const GraphDocument& document, GraphLayoutStyle style, const GraphNodeSize& size, qreal grid)
{
    const Graph g = Build(document, size);
    return style == GraphLayoutStyle::Tree ? Tree(g, grid) : Layered(g, grid);
}

} // namespace EditorCore
