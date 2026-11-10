THEMES = {
    "light": {
        "centralwidget": "rgb(245, 247, 250)",
        "top_frame":     "rgb(251, 255, 240)",
        "health_frame":  "rgb(240, 255, 254)",
        "plot_frame":    "rgb(255, 255, 255)",
        "attitude_frame":"rgb(255, 255, 255)",
        "nav_frame":     "rgb(255, 255, 255)",      # 侧边导航栏背景

        "label_color":   "#000000",
        "name_color":    "#000000",
        "button_fg":     "#000000",
        "combobox_fg":   "#000000",

        # 导航按钮专属配色
        "nav_button":       "#333333",   # 未选中时的文字颜色
        "nav_hover":        "#e8eef5",   # 鼠标悬停背景色
        "nav_checked_bg":   "#0078d4",   # 选中时的背景色（经典蓝）
        "nav_checked_fg":   "#ffffff",   # 选中时的文字颜色（纯白）

        "plot_bg":   "#ffffff",
        "plot_grid": "#d0d7de",
        "plot_text": "#666666",
    },

    "dark": {
        "centralwidget": "#0f1419",
        "top_frame":     "#141c26",
        "health_frame":  "#1a2332",
        "plot_frame":    "#1a2332",
        "attitude_frame":"#1a2332",
        "nav_frame":     "#141c26",      # 侧边导航栏背景

        "label_color":   "#e8eaed",
        "name_color":    "#00d4ff",
        "button_fg":     "#e8eaed",
        "combobox_fg":   "#e8eaed",

        # 导航按钮专属配色
        "nav_button":       "#e8eaed",   # 未选中时的文字颜色
        "nav_hover":        "#2a3a4a",   # 鼠标悬停背景色
        "nav_checked_bg":   "#00d4ff",   # 选中时的背景色（科技青）
        "nav_checked_fg":   "#0f1419",   # 选中时的文字颜色（深色，与青色对比）

        "plot_bg":   "#0f1419",
        "plot_grid": "#2a3a4a",
        "plot_text": "#8899a6",
    },
}


def button_style(theme_name: str) -> str:
    """普通按钮的圆角样式"""
    t = THEMES[theme_name]

    if theme_name == "dark":
        bg, border, hover, pressed = "#1a2332", "#2a3a4a", "#2a3a4a", "#0f1419"
    else:
        bg, border, hover, pressed = "#ffffff", "#d0d7de", "#f4f6f8", "#e1e4e8"

    return f"""
        QPushButton {{
            color: {t['label_color']};
            background-color: {bg};
            border: 1px solid {border};
            border-radius: 6px;
        }}
        QPushButton:hover {{ background-color: {hover}; }}
        QPushButton:pressed {{ background-color: {pressed}; }}
    """


def combo_style(theme_name: str) -> str:
    """下拉框样式（和按钮同底色）"""
    t = THEMES[theme_name]
    bg, border = ("#1a2332", "#2a3a4a") if theme_name == "dark" else ("#ffffff", "#d0d7de")

    return f"""
        QComboBox {{
            color: {t['label_color']};
            background-color: {bg};
            border: 1px solid {border};
            border-radius: 4px;
            padding-left: 5px;
        }}
        QComboBox QAbstractItemView {{
            background-color: {bg};
            color: {t['label_color']};
        }}
    """


def global_style(theme_name: str, face_color: str) -> str:
    """全局 QSS：Frame 背景、Label 字体、列表/文本框/进度条"""
    t = THEMES[theme_name]

    return f"""
        QLabel       {{ color: {t['label_color']}; }}
        QLabel#name  {{ color: {t['name_color']}; }}

        QFrame#top_frame       {{ background-color: {t['top_frame']}; }}
        QFrame#health_frame    {{ background-color: {t['health_frame']}; }}
        QFrame#plot_frame      {{ background-color: {t['plot_frame']}; }}
        QFrame#attitude_frame  {{ background-color: {t['attitude_frame']}; }}
        QFrame#nav_frame       {{ background-color: {t['nav_frame']}; }}
        QFrame#calib_frame     {{ background-color: {t['plot_frame']}; }}
        QFrame#about_frame     {{ background-color: {t['plot_frame']}; }}
        QFrame#line_frame      {{ background-color: {t['plot_grid']}; }}

        QFrame#faceTile {{
            border: 1px solid {t['plot_grid']};
            border-radius: 8px;
            background-color: rgba(255, 255, 255, 8);
        }}
        QFrame#faceTile[state="active"] {{ border: 2px solid {face_color}; }}
        QFrame#faceTile[state="done"]   {{ border: 2px solid {face_color}; }}

        QPlainTextEdit {{
            background-color: {t['plot_bg']};
            color: {t['label_color']};
            border: 1px solid {t['plot_grid']};
        }}

        QTextBrowser {{
            background-color: {t['plot_bg']};
            color: {t['label_color']};
            border: 1px solid {t['plot_grid']};
            padding: 10px;
        }}
        QListWidget {{
            background-color: {t['nav_frame']};
            color: {t['label_color']};
            border: 1px solid {t['plot_grid']};
            outline: none;
        }}
        QListWidget::item:selected {{
            background-color: {t['nav_checked_bg']};
            color: {t['nav_checked_fg']};
        }}

        QProgressBar {{
            background-color: {t['plot_grid']};
            border: 1px solid {t['plot_grid']};
            border-radius: 4px;
            text-align: center;
            color: {t['label_color']};
        }}
        QProgressBar::chunk {{
            background-color: {t['name_color']};
            border-radius: 3px;
        }}
    """