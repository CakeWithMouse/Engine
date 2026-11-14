#include "EditorLocale.h"
#define NOMINMAX
#include "MainWindow.h"
#include "QtHost.h"
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QSpinBox>
#include <cmath>

// Reads the physics settings on the engine thread, then opens the dialog on the UI thread.
void EditorMainWindow::ShowPhysicsSettings()
{
    if (!host_ || !host_->HasScene()) return;
    const EnginePhysicsApi* api = host_->PhysicsApi();
    if (!api)
    {
        AppendLine(Tr("Игровой модуль не поддерживает настройки физики.", "Game module does not support physics settings."));
        return;
    }
    QPointer<EditorMainWindow> self(this);
    EditorQtHost* host = host_;
    EditorCommand c;
    c.type = EditorCommand::RunTask;
    c.task = [self, host](const EngineEditSessionApi&)
    {
        const EnginePhysicsApi* physics = host->PhysicsApi();
        if (!physics) return;
        EnginePhysicsSettingsState state{};
        const uint32_t status = physics->read(physics->context, &state);
        const uint32_t available = physics->available(physics->context);
        host->RunOnUi([self, status, available, state]
        {
            if (self) self->ShowPhysicsSettingsDialog(status, available, state);
        });
    };
    host_->PostCommand(c);
}

void EditorMainWindow::ShowPhysicsSettingsDialog(uint32_t status, uint32_t available,
                                                 const EnginePhysicsSettingsState& state)
{
    if (status != EngineSceneOk)
    {
        QMessageBox::warning(this, Tr("Физика", "Physics"),
            Tr("Не удалось прочитать настройки физики (код %1).", "Failed to read physics settings (code %1).").arg(status));
        return;
    }
    QDialog dialog(this);
    dialog.setObjectName("PhysicsSettingsDialog");
    dialog.setWindowTitle(Tr("Настройки физики", "Physics Settings"));
    auto* form = new QFormLayout(&dialog);

    auto* enabled = new QCheckBox(Tr("Включить физику в Play", "Enable physics in Play"), &dialog);
    enabled->setChecked(state.enabled != 0);
    form->addRow(enabled);

    auto* gravityX = new QDoubleSpinBox(&dialog);
    auto* gravityY = new QDoubleSpinBox(&dialog);
    auto* gravityZ = new QDoubleSpinBox(&dialog);
    for (auto* box : {gravityX, gravityY, gravityZ})
    {
        box->setRange(-1000.0, 1000.0);
        box->setDecimals(2);
        box->setSingleStep(0.1);
    }
    gravityX->setValue(state.gravityX);
    gravityY->setValue(state.gravityY);
    gravityZ->setValue(state.gravityZ);
    form->addRow(Tr("Гравитация X", "Gravity X"), gravityX);
    form->addRow(Tr("Гравитация Y", "Gravity Y"), gravityY);
    form->addRow(Tr("Гравитация Z", "Gravity Z"), gravityZ);

    auto* step = new QDoubleSpinBox(&dialog);
    step->setRange(0.0001, 0.1);
    step->setDecimals(4);
    step->setSingleStep(1.0 / 60.0);
    step->setValue(state.fixedTimeStep);
    form->addRow(Tr("Фиксированный шаг, с", "Fixed timestep, s"), step);

    auto* maxSub = new QSpinBox(&dialog);
    maxSub->setRange(1, 32);
    maxSub->setValue(static_cast<int>(state.maxSubSteps));
    form->addRow(Tr("Макс. подшагов за кадр", "Max substeps per frame"), maxSub);

    auto* velocity = new QSpinBox(&dialog);
    velocity->setRange(1, 64);
    velocity->setValue(static_cast<int>(state.velocitySteps));
    form->addRow(Tr("Итерации скорости", "Velocity iterations"), velocity);

    auto* position = new QSpinBox(&dialog);
    position->setRange(1, 32);
    position->setValue(static_cast<int>(state.positionSteps));
    form->addRow(Tr("Итерации позиции", "Position iterations"), position);

    if (!available)
    {
        auto* note = new QLabel(Tr("Движок собран без Jolt Physics: значения сохраняются, но симуляция не работает.",
                                   "Engine is built without Jolt Physics: values are kept, but simulation is disabled."), &dialog);
        note->setWordWrap(true);
        form->addRow(note);
        for (auto* widget : std::initializer_list<QWidget*>{enabled, gravityX, gravityY, gravityZ, step, maxSub, velocity, position})
            widget->setEnabled(false);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]
    {
        if (!std::isfinite(gravityX->value()) || step->value() <= 0.0)
        {
            QMessageBox::warning(&dialog, Tr("Физика", "Physics"), Tr("Некорректные значения.", "Invalid values."));
            return;
        }
        EnginePhysicsSettingsState next{};
        next.enabled = enabled->isChecked() ? 1u : 0u;
        next.gravityX = static_cast<float>(gravityX->value());
        next.gravityY = static_cast<float>(gravityY->value());
        next.gravityZ = static_cast<float>(gravityZ->value());
        next.fixedTimeStep = static_cast<float>(step->value());
        next.maxSubSteps = static_cast<uint32_t>(maxSub->value());
        next.velocitySteps = static_cast<uint32_t>(velocity->value());
        next.positionSteps = static_cast<uint32_t>(position->value());

        EditorQtHost* host = host_;
        QPointer<EditorMainWindow> self(this);
        EditorCommand c;
        c.type = EditorCommand::RunTask;
        c.task = [self, host, next](const EngineEditSessionApi&)
        {
            const EnginePhysicsApi* physics = host->PhysicsApi();
            if (!physics) return;
            const uint32_t writeStatus = physics->write(physics->context, &next);
            host->RunOnUi([self, writeStatus]
            {
                if (!self) return;
                if (writeStatus != EngineSceneOk)
                    self->AppendLine(Tr("Не удалось применить настройки физики (код %1).",
                                        "Failed to apply physics settings (code %1).").arg(writeStatus));
                else
                    self->AppendLine(Tr("Настройки физики применены.", "Physics settings applied."));
            });
        };
        host_->PostCommand(c);
        dialog.accept();
    });
    dialog.exec();
}
