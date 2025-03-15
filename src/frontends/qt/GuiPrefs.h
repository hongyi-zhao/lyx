// -*- C++ -*-
/**
 * \file GuiPrefs.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author John Levon
 * \author Bo Peng
 * \author Edwin Leuven
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef GUIPREFS_H
#define GUIPREFS_H

#include "GuiDialog.h"

#include "ColorCode.h"
#include "Converter.h"
#include "Format.h"
#include "KeyMap.h"
#include "LyXRC.h"
#include "Mover.h"

#include "support/types.h"
#include "ui_PrefsUi.h"

#include "ui_PrefOutputUi.h"
#include "ui_PrefInputUi.h"
#include "ui_PrefLatexUi.h"
#include "ui_PrefScreenFontsUi.h"
#include "ui_PrefCompletionUi.h"
#include "ui_PrefColorsUi.h"
#include "ui_PrefDisplayUi.h"
#include "ui_PrefDocHandlingUi.h"
#include "ui_PrefEditUi.h"
#include "ui_PrefPathsUi.h"
#include "ui_PrefShortcutsUi.h"
#include "ui_PrefSpellcheckerUi.h"
#include "ui_PrefConvertersUi.h"
#include "ui_PrefFileformatsUi.h"
#include "ui_PrefLanguageUi.h"
#include "ui_PrefUi.h"
#include "ui_PrefIdentityUi.h"
#include "ui_ShortcutUi.h"

#include <string>
#include <vector>
#include <QtWidgets/qmenu.h>
#include <QUndoCommand>


namespace lyx {

namespace frontend {

class GuiLyXFiles;
class PrefModule;

typedef std::pair<QColor, QColor> ColorPair;
typedef std::pair<QString, QString> ColorNamePair;
typedef std::vector<ColorNamePair> ColorNamePairs;

class GuiPreferences : public GuiDialog, public Ui::PrefsUi
{
	Q_OBJECT
public:
	GuiPreferences(GuiView & lv);

	void applyRC(LyXRC & rc) const;
	void updateRC(LyXRC const & rc);

public Q_SLOTS:
	void change_adaptor();
	void slotFileSelected(QString);

Q_SIGNALS:
	void prefsApplied(LyXRC const & rc);

public:
	/// Apply changes
	void applyView() override;

	std::vector<PrefModule *> modules_;

	///
	bool initialiseParams(std::string const &) override;
	///
	void clearParams() override {}
	///
	void dispatchParams() override;
	///
	bool isBufferDependent() const override { return false; }

	/// various file pickers
	QString browsebind(QString const & file);
	QString browseUI(QString const & file);
	QString browsekbmap(QString const & file);

	/// general browse
	QString browse(QString const & file, QString const & title);

	/// set a color
	void setColor(ColorCode col, ColorNamePair const & hex);

	LyXRC & rc() { return rc_; }
	Converters & converters() { return converters_; }
	Formats & formats() { return formats_; }
	Movers & movers() { return movers_; }

private:
	///
	void addModule(PrefModule * module);
	///
	QString browseLibFile(QString const & dir,
		QString const & name, QString const & ext);

	/// temporary lyxrc
	LyXRC rc_;
	/// temporary converters
	Converters converters_;
	/// temporary formats
	Formats formats_;
	/// temporary movers
	Movers movers_;

	/// A list of colors to be dispatched
	std::vector<std::string> colors_;
	/// UI file selector
	GuiLyXFiles * guilyxfiles_;
	/// Selected UI file
	QString uifile_;
};


class PrefModule : public QWidget
{
	Q_OBJECT
public:
	PrefModule(QString const & cat, QString const & t,
			GuiPreferences * form)
		: QWidget(form), category_(cat), title_(t), form_(form)
	{}

	virtual void applyRC(LyXRC & rc) const = 0;
	virtual void updateRC(LyXRC const & rc) = 0;

	QString const & category() const { return category_; }
	QString const & title() const { return title_; }

protected:
	QString category_;
	QString title_;
	GuiPreferences * form_;

Q_SIGNALS:
	void changed();
};


class PrefOutput : public PrefModule, public Ui::PrefOutputUi
{
	Q_OBJECT
public:
	PrefOutput(GuiPreferences * form);

	void applyRC(LyXRC & rc) const override;
	void updateRC(LyXRC const & rc) override;
};


class PrefInput : public PrefModule, public Ui::PrefInputUi
{
	Q_OBJECT
public:
	PrefInput(GuiPreferences * form);

	void applyRC(LyXRC & rc) const override;
	void updateRC(LyXRC const & rc) override;

private Q_SLOTS:
	void on_firstKeymapPB_clicked(bool);
	void on_secondKeymapPB_clicked(bool);
	void on_keymapCB_toggled(bool);
	void on_scrollzoomEnableCB_toggled(bool);

private:
	QString testKeymap(QString const & keymap);
};


class PrefCompletion : public PrefModule, public Ui::PrefCompletionUi
{
	Q_OBJECT
public:
	PrefCompletion(GuiPreferences * form);

	void applyRC(LyXRC & rc) const override;
	void updateRC(LyXRC const & rc) override;
	virtual void enableCB();
private Q_SLOTS:
	void on_popupTextCB_clicked();
	void on_inlineTextCB_clicked();
};


class PrefLatex : public PrefModule, public Ui::PrefLatexUi
{
	Q_OBJECT
public:
	PrefLatex(GuiPreferences * form);

	void applyRC(LyXRC & rc) const override;
	void updateRC(LyXRC const & rc) override;

private Q_SLOTS:
	void on_latexBibtexCO_activated(int n);
	void on_latexJBibtexCO_activated(int n);
	void on_latexIndexCO_activated(int n);

private:
	///
	std::set<std::string> bibtex_alternatives;
	///
	std::set<std::string> jbibtex_alternatives;
	///
	std::set<std::string> index_alternatives;
};


class PrefScreenFonts : public PrefModule, public Ui::PrefScreenFontsUi
{
	Q_OBJECT
public:
	PrefScreenFonts(GuiPreferences * form);

	void applyRC(LyXRC & rc) const override;
	void updateRC(LyXRC const & rc) override;

private Q_SLOTS:
	void selectRoman(const QString&);
	void selectSans(const QString&);
	void selectTypewriter(const QString&);
	void screenFontsChanged();

public Q_SLOTS:
	void updateScreenFontSizes(LyXRC const & rc);
};


class PrefColors : public PrefModule, public Ui::PrefColorsUi
{
	Q_OBJECT

public:
	PrefColors(GuiPreferences * form);

	void applyRC(LyXRC & rc) const override;
	void updateRC(LyXRC const & rc) override;

private Q_SLOTS:
	void changeLightColor(){ changeColor(false); }
	void changeDarkColor() { changeColor(true);  }
	void changeColor();
	void changeColor(int const row, int const column);
	void resetColor();
	void resetAllColor();
	void redrawColorTable();
	void changeSysColor();
	void changeLyxObjectsSelection();
	void changeAutoapply();
	bool setColor(int const row, ColorPair const & new_colors,
	              ColorNamePair const & old_colors);
	bool setColor(int const row, bool const dark_mode, QColor const & new_color,
	              QString const & old_color);
	bool isDefaultColor(int const row, ColorNamePair const & color);
	void setDisabledResets();
	void openThemeMenu();
	void saveThemeInterface();
	void loadThemeInterface(QListWidgetItem* current_item,
	                        QListWidgetItem* previous_item);
	void removeTheme();
	void exportThemeInterface();
	void importThemeInterface();

	void pressCurrentItem(QTableWidgetItem * item = nullptr);
	void changeFocus();

	void searchColorItem(bool backward_direction);
	void searchNextColorItem();
	void searchPreviousColorItem();

private:
	///
	void changeColor(bool const dark_color);
	///
	ColorPair getDefaultColorsByRow(int const row);
	///
	void setIcons(size_type row, ColorPair colors);
	///
	void setIcon(size_type row, bool const dark_mode, QColor color);
	///
	void updateAllIcons();
	///
	void initializeThemesLW();
	///
	void initializeThemeMenu();
	///
	void saveTheme(QString file_path);
	///
	void loadTheme(support::FileName filename);
	///
	bool askThemeName(bool porting);
	///
	bool wantToOverwrite();
	///
	ColorPair toqcolor(ColorNamePair);

	std::vector<ColorCode> lcolors_;
	ColorNamePairs curcolors_;
	ColorNamePairs newcolors_;

	QList<QTableWidgetItem *> items_found_;
	QList<QTableWidgetItem *>::iterator it_;
	QString search_string_;

	int const icon_width_  = 36;
	int const icon_height_ = 18;

	bool autoapply_ = false;
	QUndoStack * undo_stack_;

	QMenu theme_menu_;
	std::vector<bool> isSysThemes_;
	std::vector<QString> theme_fullpaths_;
	/// holds currently selected theme
	QString theme_name_ = "";
	/// holds filename of currently selected theme
	QString theme_filename_;

	friend class SetColor;
};


class PrefDisplay : public PrefModule, public Ui::PrefDisplayUi
{
	Q_OBJECT
public:
	PrefDisplay(GuiPreferences * form);

	void applyRC(LyXRC & rc) const override;
	void updateRC(LyXRC const & rc) override;

private Q_SLOTS:
	void on_instantPreviewCO_currentIndexChanged(int);
};


class PrefPaths : public PrefModule, public Ui::PrefPathsUi
{
	Q_OBJECT
public:
	PrefPaths(GuiPreferences * form);

	void applyRC(LyXRC & rc) const override;
	void updateRC(LyXRC const & rc) override;

private Q_SLOTS:
	void selectExampledir();
	void selectTemplatedir();
	void selectTempdir();
	void selectBackupdir();
	void selectWorkingdir();
	void selectThesaurusdir();
	void selectHunspelldir();
	void selectLyxPipe();

};


class PrefSpellchecker : public PrefModule, public Ui::PrefSpellcheckerUi
{
	Q_OBJECT
public:
	PrefSpellchecker(GuiPreferences * form);

	void applyRC(LyXRC & rc) const override;
	void updateRC(LyXRC const & rc) override;

private Q_SLOTS:
	void on_spellcheckerCB_currentIndexChanged(int);
};


class PrefConverters : public PrefModule, public Ui::PrefConvertersUi
{
	Q_OBJECT
public:
	PrefConverters(GuiPreferences * form);

	void applyRC(LyXRC & rc) const override;
	void updateRC(LyXRC const & rc) override;

public Q_SLOTS:
	void updateGui();

private Q_SLOTS:
	void updateConverter();
	void switchConverter();
	void removeConverter();
	void changeConverter();
	void on_cacheCB_stateChanged(int state);
	void on_needauthForbiddenCB_toggled(bool);
	void on_needauthCB_toggled(bool);

private:
	void updateButtons();
};


class PrefFileformats : public PrefModule, public Ui::PrefFileformatsUi
{
	Q_OBJECT
public:
	PrefFileformats(GuiPreferences * form);

	void applyRC(LyXRC & rc) const override;
	void updateRC(LyXRC const & rc) override;
	void updateView();

Q_SIGNALS:
	void formatsChanged();

private Q_SLOTS:
	void on_copierED_textEdited(const QString & s);
	void on_extensionsED_textEdited(const QString &);
	void on_viewerED_textEdited(const QString &);
	void on_editorED_textEdited(const QString &);
	void on_mimeED_textEdited(const QString &);
	void on_shortcutED_textEdited(const QString &);
	void on_formatED_editingFinished();
	void on_formatED_textChanged(const QString &);
	void on_formatsCB_currentIndexChanged(int);
	void on_formatsCB_editTextChanged(const QString &);
	void on_formatNewPB_clicked();
	void on_formatRemovePB_clicked();
	void on_viewerCO_currentIndexChanged(int i);
	void on_editorCO_currentIndexChanged(int i);
	void setFlags();
	void updatePrettyname();

private:
	Format & currentFormat();
	///
	void updateViewers();
	///
	void updateEditors();
	///
	LyXRC::Alternatives viewer_alternatives;
	///
	LyXRC::Alternatives editor_alternatives;
};


class PrefLanguage : public PrefModule, public Ui::PrefLanguageUi
{
	Q_OBJECT
public:
	PrefLanguage(GuiPreferences * form);

	void applyRC(LyXRC & rc) const override;
	void updateRC(LyXRC const & rc) override;

private Q_SLOTS:
	void on_uiLanguageCO_currentIndexChanged(int);
	void on_languagePackageCO_currentIndexChanged(int);
	void on_defaultDecimalSepCO_currentIndexChanged(int);
private:
	///
	QString save_langpack_;
};


class PrefUserInterface : public PrefModule, public Ui::PrefUi
{
	Q_OBJECT
public:
	PrefUserInterface(GuiPreferences * form);

	void applyRC(LyXRC & rc) const override;
	void updateRC(LyXRC const & rc) override;

public Q_SLOTS:
	void selectUi();
};


class PrefDocHandling : public PrefModule, public Ui::PrefDocHandlingUi
{
	Q_OBJECT
public:
	PrefDocHandling(GuiPreferences * form);

	void applyRC(LyXRC & rc) const override;
	void updateRC(LyXRC const & rc) override;

public Q_SLOTS:
	void on_clearSessionPB_clicked();
};



class PrefEdit : public PrefModule, public Ui::PrefEditUi
{
	Q_OBJECT
public:
	PrefEdit(GuiPreferences * form);

	void applyRC(LyXRC & rc) const override;
	void updateRC(LyXRC const & rc) override;

public Q_SLOTS:
	void on_screenLimitCB_toggled(bool);
	void on_citationSearchCB_toggled(bool);
};



class GuiShortcutDialog : public QDialog, public Ui::shortcutUi
{
	Q_OBJECT
public:
	GuiShortcutDialog(QWidget * parent);

public Q_SLOTS:
	void on_lfunLE_textChanged();

};


class PrefShortcuts : public PrefModule, public Ui::PrefShortcuts
{
	Q_OBJECT
public:
	PrefShortcuts(GuiPreferences * form);

	void applyRC(LyXRC & rc) const override;
	void updateRC(LyXRC const & rc) override;
	void updateShortcutsTW();

public Q_SLOTS:
	void selectBind();
	void on_modifyPB_pressed();
	void on_newPB_pressed();
	void on_removePB_pressed();
	void on_searchLE_textEdited();
	///
	void on_shortcutsTW_itemSelectionChanged();
	void on_shortcutsTW_itemDoubleClicked();
	///
	void shortcutOkPressed();
	void shortcutCancelPressed();
	void shortcutClearPressed();
	void shortcutRemovePressed();

private:
	void modifyShortcut();
	/// remove selected binding, restore default value
	void removeShortcut();
	/// remove bindings, do not restore default values
	void deactivateShortcuts(QList<QTreeWidgetItem*> const & items);
	/// check the new binding k->func, and remove existing bindings to k after
	/// asking the user. We exclude lfun_to_modify from this test: we assume
	/// that if the user clicked "modify" then they agreed to modify the
	/// binding. Returns false if the shortcut is invalid or the user cancels.
	bool validateNewShortcut(FuncRequest const & func,
	                         KeySequence const & k,
	                         QString const & lfun_to_modify);
	/// compute current active shortcut
	FuncRequest currentBinding(KeySequence const & k);
	///
	void setItemType(QTreeWidgetItem * item, KeyMap::ItemType tag);
	///
	static KeyMap::ItemType itemType(QTreeWidgetItem & item);
	/// some items need to be always hidden, for instance empty rebound
	/// system keys
	static bool isAlwaysHidden(QTreeWidgetItem & item);
	/// unhide an empty system binding that may have been hidden
	/// returns either null or the unhidden shortcut
	void unhideEmpty(QString const & lfun, bool select);
	///
	QTreeWidgetItem * insertShortcutItem(FuncRequest const & lfun,
		KeySequence const & shortcut, KeyMap::ItemType tag);
	///
	GuiShortcutDialog * shortcut_;
	///
	ButtonController shortcut_bc_;
	/// category items
	QTreeWidgetItem * editItem_;
	QTreeWidgetItem * mathItem_;
	QTreeWidgetItem * bufferItem_;
	QTreeWidgetItem * layoutItem_;
	QTreeWidgetItem * systemItem_;
	// system_bind_ holds bindings from rc.bind_file
	// user_bind_ holds \bind bindings from user.bind
	// user_unbind_ holds \unbind bindings from user.bind
	// When an item is inserted, it is added to user_bind_
	// When an item from system_bind_ is deleted, it is added to user_unbind_
	// When an item in user_bind_ or user_unbind_ is deleted, it is
	//	deleted (unbind)
	KeyMap system_bind_;
	KeyMap user_bind_;
	KeyMap user_unbind_;
	///
	QString save_lfun_;
};


class PrefIdentity : public PrefModule, public Ui::PrefIdentityUi
{
	Q_OBJECT
public:
	PrefIdentity(GuiPreferences * form);

	void applyRC(LyXRC & rc) const override;
	void updateRC(LyXRC const & rc) override;
};


class SetColor : public PrefColors, public QUndoCommand
{
public:
	SetColor(const int row, bool dark_mode, const QColor & new_color,
	         QString old_color,
	         ColorNamePairs & new_color_list,
	         const bool autoapply, PrefColors* color_module,
	         QUndoCommand* uc_parent = nullptr);
	~SetColor(){};

	void redo() override;
	void undo() override;
	void setColor(QColor);

private:
	const bool autoapply_;
	const int row_;
	const bool dark_mode_;
	QColor new_color_;
	QString old_color_;
	ColorNamePairs & newcolors_;
	PrefColors* parent_;
};


} // namespace frontend
} // namespace lyx

#endif // GUIPREFS_H
