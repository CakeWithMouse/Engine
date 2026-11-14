#pragma once

#include <QString>

// Тексты ошибок визуальных скриптов приходят из игрового модуля по-английски: в расширении
// Engine.Scene.Script одно поле message, а язык редактора движок не знает. Редактор переводит
// известные тексты по шаблонам; неизвестный (например, из более новой Game.dll) остаётся как есть.
namespace EditorCore
{
// Русский текст для английского сообщения движка; исходный текст, если шаблон не найден.
QString ScriptMessageRu(const QString& message);

// Сообщение на текущем языке редактора.
QString LocalizeScriptMessage(const QString& message);
}
