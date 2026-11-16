#pragma once

#include <QString>
#include <QStringList>

namespace EditorCore
{

// История правок графа снимками JSON. Снимок кладётся до правки; правки с одинаковым ключом подряд
// (тянущийся ползунок, снятие и новое подключение связи) — один шаг отмены.
class GraphHistory
{
public:
    static constexpr int Limit = 200;

    void Clear();
    bool CanUndo() const { return !undo_.isEmpty(); }
    bool CanRedo() const { return !redo_.isEmpty(); }

    // Правка с ключом mergeKey продолжает предыдущую с тем же ключом: снимок не нужен.
    bool Merges(const QString& mergeKey) const { return !mergeKey.isEmpty() && mergeKey == lastKey_; }
    // Снимок до правки; повтор последнего снимка не дублируется. Повтор (redo) сбрасывается.
    void Push(const QString& snapshot, const QString& mergeKey = {});
    // Следующая правка не сливается с предыдущей.
    void BreakMerge() { lastKey_.clear(); }
    const QString& LastKey() const { return lastKey_; }

    // current — граф сейчас; restored — снимок, который надо показать. false: шагать некуда.
    bool Undo(const QString& current, QString& restored);
    bool Redo(const QString& current, QString& restored);

    // Последний шаг с ключом mergeKey ничего не поменял (снимок равен current): он убирается.
    bool DropEmptyStep(const QString& mergeKey, const QString& current);

private:
    QStringList undo_, redo_;
    QString lastKey_;
};

} // namespace EditorCore
