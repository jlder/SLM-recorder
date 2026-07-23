/*******************************************************************************
 * STYLE SHEET
 * All CSS used by the embedded web interface. Kept embedded so the page does not depend on SD/FATFS files.
 *
 * Byte-stability rule: this file contains one raw string literal fragment.
 * Do not add comments or whitespace inside the raw string literal
 * block unless the generated web page is intentionally being changed.
 ******************************************************************************/

R"rawliteral(    <style>
        /* Global page shell and base typography. */
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: Arial, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 10px;
            font-size: 16px;
        }
        .container {
            max-width: 1000px;
            margin: 0 auto;
            background: white;
            border-radius: 8px;
            box-shadow: 0 8px 28px rgba(0,0,0,0.12);
            overflow: hidden;
        }
        .header {
            background: #2c3e50;
            color: white;
            padding: 12px;
            text-align: center;
        }
        .header h1 { font-size: 20px; line-height: 1.2; }
        .info-bar {
            background: #34495e;
            color: white;
            padding: 8px 10px;
            display: flex;
            justify-content: space-around;
            flex-wrap: wrap;
            gap: 4px 10px;
        }
        .info-item { font-size: 13px; padding: 2px; }
        .info-value { font-weight: bold; margin-left: 4px; }
        .controls {
            padding: 10px;
            background: #ecf0f1;
            text-align: center;
        }
        .section { padding: 12px; }
        h2 { font-size: 18px; margin: 2px 0 8px; }
        p { margin: 6px 0; line-height: 1.35; }
        .hidden { display: none; }

        /* Shared button colors: blue for normal actions, amber for required
           maintenance/calibration actions, green for returns, grey when disabled. */
        .btn {
            padding: 7px 10px;
            margin: 2px;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            font-size: 13px;
            font-weight: bold;
            min-height: 34px;
        }
        .btn-primary { background: #3498db; color: white; }
        .btn-warning { background: #f39c12; color: white; }
        .btn:disabled { background: #bdc3c7; color: #666; cursor: not-allowed; }
        .delete-warning-window {
            border: 1px solid #f1c40f;
            background: #fff8e1;
            color: #5d4600;
            border-radius: 8px;
            padding: 10px 12px;
            margin: 8px 0;
        }
        .btn-return { background: #27ae60; color: white; }

        /* File, report, and processed-delete lists use the same compact rows. */
        input[type="password"], input[type="file"] {
            max-width: 100%;
            min-height: 40px;
            font-size: 16px;
        }
        .file-list { margin-top: 8px; }
        .file-item {
            border-bottom: 1px solid #ecf0f1;
            padding: 8px 10px;
        }
        .file-item:hover { background: #f8f9fa; }
        .file-row {
            display: grid;
            grid-template-columns: minmax(0, 1fr) auto;
            gap: 8px;
            align-items: center;
            min-height: 34px;
        }
        .file-name {
            font-family: Consolas, monospace;
            font-size: 14px;
            overflow-wrap: anywhere;
        }
        .file-size { font-size: 13px; color: #555; }

        /* Browser-side flight-analysis panel shown after a recorder file download. */
        .analysis-window {
            border: 1px solid #3498db;
            background: #eef7ff;
            border-radius: 8px;
            padding: 10px;
            margin: 10px 0;
        }
        .analysis-window h2 { margin-bottom: 6px; }
        .progress-block {
            margin: 8px 0;
        }
        .progress-label {
            font-size: 12px;
            margin-bottom: 3px;
        }
        .progress-track {
            width: 100%;
            height: 14px;
            border: 1px solid #95a5a6;
            border-radius: 7px;
            overflow: hidden;
            background: #ecf0f1;
        }
        .progress-fill {
            height: 100%;
            width: 0%;
            background: #3498db;
            transition: width 0.15s linear;
        }
        .analysis-table {
            width: 100%;
            border-collapse: collapse;
            table-layout: fixed;
            margin-top: 8px;
        }
        .analysis-table th {
            background: #2c3e50;
            color: white;
            padding: 5px 2px;
            text-align: center;
            font-size: 11px;
            line-height: 1.05;
        }
        .analysis-table td {
            padding: 6px 2px;
            border-bottom: 1px solid #dfe6e9;
            font-size: 11px;
            text-align: center;
            white-space: nowrap;
        }
        .flight-log-view {
            white-space: pre-wrap;
            overflow-wrap: anywhere;
            background: white;
            border: 1px solid #dfe6e9;
            border-radius: 6px;
            padding: 8px;
            margin-top: 8px;
            font-size: 12px;
            line-height: 1.35;
            color: #2c3e50;
        }
        .file-btn {
            width: 96px;
            min-height: 32px;
            padding: 6px 8px;
            margin: 1px 0;
            font-size: 13px;
        }
        .delete-file-row {
            display: grid;
            grid-template-columns: auto minmax(0, 1fr) auto;
            gap: 8px;
            align-items: center;
            min-height: 36px;
        }
        .delete-file-row input[type="checkbox"] {
            width: 22px;
            height: 22px;
        }

        /* Calibration tables and face chips display live recorder/installation status. */
        .cal-table {
            width: 100%;
            border-collapse: collapse;
            margin-top: 8px;
        }
        .cal-table th {
            background: #2c3e50;
            color: white;
            padding: 8px;
            text-align: left;
            font-size: 13px;
        }
        .cal-table td {
            padding: 8px;
            border-bottom: 1px solid #ecf0f1;
            font-size: 13px;
        }
        .loading { text-align: center; padding: 24px; color: #7f8c8d; }
        .status-recording { color: #2ecc71; font-weight: bold; }
        .card {
            background: #f8f9fa;
            border: 1px solid #dfe6e9;
            border-radius: 8px;
            padding: 10px;
            margin: 8px 0;
        }
        .ok { color: #27ae60; font-weight: bold; }
        .warn { color: #f39c12; font-weight: bold; }
        .bad { color: #c0392b; font-weight: bold; }
        .mono { font-family: Consolas, monospace; }
        .small { font-size: 13px; color: #555; }
        .face-summary-grid {
            display: grid;
            grid-template-columns: repeat(3, minmax(58px, 1fr));
            gap: 6px;
            margin-top: 4px;
        }
        .face-chip {
            display: flex;
            justify-content: space-between;
            align-items: center;
            gap: 5px;
            padding: 5px 7px;
            border: 1px solid #dfe6e9;
            border-radius: 5px;
            background: #fff;
            color: #111;
            font-weight: bold;
        }
        .face-chip-warn { color: #f39c12; border-color: #f39c12; }
        .face-chip-ok { color: #27ae60; border-color: #27ae60; }
        .face-chip-active { border-width: 4px; padding: 2px 4px; }

        /* Button grids keep maintenance and calibration actions usable on touch screens. */
        .button-grid {
            display: grid;
            grid-template-columns: repeat(2, minmax(120px, 180px));
            gap: 8px;
            margin: 8px 0;
            justify-content: center;
        }
        .main-menu-grid .btn, .maintenance-grid .btn {
            min-height: 42px;
        }
        .main-menu-grid {
            grid-template-columns: repeat(3, minmax(90px, 160px));
        }
        .cal-menu-item { margin-bottom: 10px; text-align: center; }
        .cal-menu-item .small { margin-top: 3px; }
        .last-cal-heading {
            text-align: center;
            font-weight: bold;
            color: #2c3e50;
            margin: 5px 0 2px;
            line-height: 1.15;
        }
        .last-cal-date {
            text-align: center;
            line-height: 1.2;
        }
        .cal-actions {
            display: grid;
            grid-template-columns: repeat(2, minmax(100px, 140px));
            gap: 6px;
            justify-content: center;
            text-align: initial;
            margin: 8px auto;
        }
        .cal-actions .btn, .button-grid .btn {
            width: 100%;
            margin: 0;
        }
        .workflow-card { font-size: 13px; }
        .workflow-card p { line-height: 1.3; }
        .cal-status-card { font-size: 13px; line-height: 1.3; }
        .candidate {
            font-size: 16px;
            margin: 8px 0;
        }
        /* Log/status windows for calibration and support actions. */
        pre {
            white-space: pre-wrap;
            background: #2c3e50;
            color: #ecf0f1;
            padding: 8px;
            border-radius: 5px;
            max-height: 160px;
            overflow: auto;
            font-size: 13px;
        }
        /* Narrow-screen adjustments for phone use when connected to the recorder AP. */
        @media (max-width: 600px) {
            body { padding: 6px; font-size: 15px; }
            .container { border-radius: 6px; }
            .header { padding: 10px 8px; }
            .header h1 { font-size: 18px; }
            .section { padding: 10px; }
            .controls { padding: 8px; }
            .controls .btn, .card > .btn { width: 100%; }
            .cal-actions .btn, .button-grid .btn { width: 100%; }
            .card { padding: 9px; }
            .file-item { padding: 7px 8px; }
            .file-name { font-size: 13px; }
            .file-btn { width: 88px; font-size: 12px; }
            .cal-table { display: block; overflow-x: auto; white-space: nowrap; }
            .face-summary-grid { grid-template-columns: repeat(2, minmax(58px, 1fr)); }
        }
    </style>
</head>
)rawliteral"
