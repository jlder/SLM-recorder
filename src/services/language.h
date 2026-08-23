// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 AgingGliders

/**
 * @file src/services/language.h
 * @brief Central English/French user-interface text catalog.
 *
 * All human-facing recorder text belongs in this catalog. Protocol tokens,
 * JSON keys/reason codes, NVS keys, filenames, file-format markers, and
 * persistent calibration-report parser strings deliberately remain outside it.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

// French is value zero so erased/cleared/default settings naturally select it.
typedef enum : uint8_t {
  LANGUAGE_FRENCH = 0u,
  LANGUAGE_ENGLISH = 1u
} language_t;

// Single source of truth for every translated recorder UI string.
// X(symbolic_id, English, French)
#define LANGUAGE_TEXT_LIST(X) \
  X(EMPTY, "", "") \
  X(MENU, "MENU", "MENU") \
  X(BATTERY_LOW_USB, "BATTERY LOW\\nRECHARGE WITH USB", "BATTERIE FAIBLE\\nRECHARGER PAR USB") \
  X(START_RECORD, "START RECORD", "DÉBUT ENREG.") \
  X(STOP_RECORD, "STOP RECORD", "ARRÊT ENREG.") \
  X(START_WIFI, "START WIFI", "ACTIVER WIFI") \
  X(STOP_WIFI, "STOP WIFI", "DÉSACTIV. WIFI") \
  X(SETTINGS, "SETTINGS", "RÉGLAGES") \
  X(BACK, "BACK", "RETOUR") \
  X(DATE, "DATE", "DATE") \
  X(TIME, "TIME", "HEURE") \
  X(REGISTRATION, "REGISTRATION", "IMMATRICUL.") \
  X(AUTOMATION, "AUTOMATION", "AUTOMATISME") \
  X(AUTO_RECORDING, "AUTO RECORDING", "ENREG. AUTO") \
  X(AUTO_WIFI, "AUTO WIFI", "WIFI AUTO") \
  X(AUTO_DELETE, "AUTO DELETE", "SUPPR. AUTO") \
  X(ON, "ON", "ON") \
  X(OFF, "OFF", "OFF") \
  X(SET_DATE, "SET DATE", "RÉGLER DATE") \
  X(YEAR, "Year", "Année") \
  X(MONTH, "Month", "Mois") \
  X(DAY, "Day", "Jour") \
  X(SAVE, "SAVE", "VALIDER") \
  X(SET_TIME, "SET TIME", "RÉGLER HEURE") \
  X(HOUR, "Hour", "Heure") \
  X(MINUTE, "Minute", "Minute") \
  X(SET_REGISTRATION, "SET REGISTRATION", "IMMATRICULATION") \
  X(REGISTRATION_HINT, "5 uppercase letters/digits", "5 caractères A-Z / 0-9") \
  X(SW_VERSION_LABEL, "sw ver", "ver. logic.") \
  X(HW_VERSION_LABEL, "hw ver", "ver. mat.") \
  X(SWITCH_TO_ENGLISH, "ENGLISH", "ENGLISH") \
  X(SWITCH_TO_FRENCH, "FRANÇAIS", "FRANÇAIS") \
  X(BOOT, "BOOT", "DÉMARRAGE") \
  X(READY, "READY", "PRÊT") \
  X(RECORDING, "RECORDING", "ENREGIST.") \
  X(STARTING, "STARTING", "LANCEMENT") \
  X(STOPPING, "STOPPING", "ARRÊT") \
  X(NEED_SETTINGS, "NEED SETTINGS", "RÉGLAGES REQ.") \
  X(REC_CAL_REQ, "REC CAL REQ", "CAL.ENREG.REQ.") \
  X(INST_CAL_REQ, "INST CAL REQ", "CAL.INST.REQ.") \
  X(REC_CAL_FAULT, "REC CAL FAULT", "DÉF. CAL. REC.") \
  X(ACCEL_ERR, "ACCEL ERR", "ERR. ACCEL.") \
  X(RTC_ERROR, "RTC ERROR", "ERR. RTC") \
  X(PMU_ERROR, "PMU ERROR", "ERR. PMU") \
  X(REC_FAIL, "REC FAIL", "ÉCHEC ENREG.") \
  X(TOUCH_ERROR, "TOUCH ERROR", "ERR. TACTILE") \
  X(ERROR, "ERROR", "ERREUR") \
  X(NO_SD, "NO SD", "PAS DE SD") \
  X(SD_LOW, "SD LOW", "SD PLEINE") \
  X(SD_FULL_FILES, "TOO MANY FILES", "TROP FICH. SD") \
  X(SD_FILE_ERR, "SD FILE ERR", "ERR. FICH. SD") \
  X(SD_OK_CLR, "SD OK/CLR", "SD OK/EFF.") \
  X(LOW_BATT, "LOW BATT", "BATT. FAIBLE") \
  X(SHUTDOWN, "SHUTDOWN", "ARRÊT") \
  X(GENERIC_ERROR, "GENERIC ERROR", "ERREUR GÉNÉR.") \
  X(FATAL_WDG_CLR, "FATAL WDG/CLR", "FATAL WDG/EFF.") \
  X(PAGE_RECORDER, "SLM Recorder", "SLM Recorder") \
  X(PAGE_FILE_MANAGEMENT, "SLM File Management", "SLM - Gestion fichiers") \
  X(PAGE_LOGBOOK, "SLM Logbook", "SLM - Carnet de vol") \
  X(PAGE_MAINTENANCE, "SLM Maintenance", "SLM - Maintenance") \
  X(PAGE_RECORDER_CAL, "SLM Recorder Calibration", "SLM - Calibration enregistreur") \
  X(PAGE_INSTALL_CAL, "SLM Installation Calibration", "SLM - Calibration installation") \
  X(PAGE_REPORTS, "SLM Report Management", "SLM - Gestion rapports") \
  X(PAGE_ABOUT, "SLM About", "SLM - À propos") \
  X(PAGE_FIRMWARE, "SLM Firmware Update", "SLM - Mise à jour logiciel") \
  X(MAIN_MENU, "Main Menu", "Menu principal") \
  X(FILE_MANAGEMENT, "File Management", "Gestion fichiers") \
  X(LOGBOOK, "Logbook", "Carnet de vol") \
  X(MAINTENANCE, "Maintenance", "Maintenance") \
  X(USB_POWER_REQUIRED, "USB power required.", "Alimentation USB requise.") \
  X(USB_POWER_FUNCTIONS, "Connect USB power to use all recorder functions except Recorder Calibration.", "Brancher USB pour utiliser toutes les fonctions sauf la calibration de l'enregistreur.") \
  X(LOGBOOK_PROCESSED_ONLY, "Only files downloaded through the SLM app are included in the logbook.", "Seuls les fichiers déjà traités sont inclus dans le carnet de vol.") \
  X(LOGBOOK_LAST_FIVE_DAYS, "Only the last 5 flying days are displayed.", "Seuls les 5 derniers jours de vol sont affichés.") \
  X(RETURN, "Return", "Retour") \
  X(LOADING_LOGBOOK, "Loading logbook...", "Chargement du carnet de vol...") \
  X(SD_CARD_LABEL, "SD Card:", "Carte SD :") \
  X(FREE_LABEL, "Free:", "Libre :") \
  X(FILES_LABEL, "Files:", "Fichiers :") \
  X(LOADING_FILES, "Loading files...", "Chargement des fichiers...") \
  X(FLIGHT_ANALYSIS, "SLM Flight Analysis", "Analyse des vols SLM") \
  X(NO_FLIGHT_ANALYSIS, "No flight analysis available.", "Aucune analyse de vol disponible.") \
  X(MAINTENANCE_ACCESS, "Maintenance Access", "Accès maintenance") \
  X(MAINTENANCE_RESTRICTED, "Maintenance is restricted. Enter the password to unlock maintenance functions.", "L'accès maintenance est restreint. Entrer le mot de passe pour accéder aux fonctions de maintenance.") \
  X(PASSWORD, "Password", "Mot de passe") \
  X(UNLOCK_MAINTENANCE, "Unlock Maintenance", "Déverrouiller maintenance") \
  X(LOCKED, "Locked.", "Verrouillé.") \
  X(CHECKING, "Checking...", "Vérification...") \
  X(UNLOCKED, "Unlocked.", "Déverrouillé.") \
  X(WRONG_MAINTENANCE_PASSWORD, "Wrong password or maintenance access denied.", "Mot de passe incorrect ou accès maintenance refusé.") \
  X(UNLOCK_ERROR, "Unlock error", "Erreur de déverrouillage") \
  X(RECORDER_CALIBRATION, "Recorder Calibration", "Calibration enregistreur") \
  X(LAST_RECORDER, "Last Recorder", "Dernière calibration") \
  X(CALIBRATION_WORD, "Calibration", "enregistreur") \
  X(INSTALLATION_CALIBRATION, "Installation Calibration", "Calibration installation") \
  X(LAST_INSTALLATION, "Last Installation", "Dernière calibration") \
  X(INSTALLATION_WORD, "Calibration", "installation") \
  X(REPORT_MANAGEMENT, "Report Management", "Gestion rapports") \
  X(FIRMWARE_UPDATE, "Firmware Update", "Mise à jour logiciel") \
  X(ABOUT, "About", "À propos") \
  X(WORKFLOW, "Workflow:", "Procédure :") \
  X(REC_CAL_WORKFLOW, "Start recorder calibration, place the recorder still on each of its six faces, and wait for each face to show OK. The recorder automatically keeps the best capture for each face. Save when all six face values are satisfactory. Initial setup requires two consecutive calibrations.", "Démarrer la calibration enregistreur, poser l'enregistreur immobile sur chacune de ses six faces et attendre que chaque face affiche OK. L'enregistreur conserve automatiquement la meilleure mesure pour chaque face. Valider lorsque les six faces sont satisfaisantes. La première configuration impose deux calibrations successives.") \
  X(START, "Start", "Démarrer") \
  X(CANCEL, "Cancel", "Annuler") \
  X(STATUS_LABEL, "Status:", "État :") \
  X(SAMPLES_LABEL, "Samples:", "Échantillons :") \
  X(SENSOR_TEMPERATURE_LABEL, "Sensor temperature:", "Température capteur :") \
  X(STDDEV_CURRENT_MIN_LABEL, "Stddev current/min:", "Écart type actuel/min :") \
  X(TIME_SINCE_MIN_LABEL, "Time since min:", "Temps depuis min :") \
  X(FACES_LABEL, "Faces:", "Faces :") \
  X(AXIS, "Axis", "Axe") \
  X(GAIN, "Gain", "Gain") \
  X(NVS_GAIN, "NVS Gain", "Gain NVS") \
  X(OFFSET, "Offset", "Décalage") \
  X(NVS_OFFSET, "NVS Offset", "Décalage NVS") \
  X(READY_DOT, "Ready.", "Prêt.") \
  X(VALID_SINCE, "valid since", "valide depuis") \
  X(REPEAT_REC_CAL_REQUIRED, "repeat recorder calibration required", "nouvelle calibration enregistreur requise") \
  X(FIRST_REC_CAL_REQUIRED, "first recorder calibration required", "première calibration enregistreur requise") \
  X(FAULT_PREFIX, "fault:", "défaut :") \
  X(MISSING, "missing", "absente") \
  X(STABLE, "stable", "stable") \
  X(NOT_STABLE, "not stable", "instable") \
  X(OK, "OK", "OK") \
  X(UNAVAILABLE, "UNAVAILABLE", "INDISPONIBLE") \
  X(TOO_LOW, "TOO LOW", "TROP BASSE") \
  X(TOO_HIGH, "TOO HIGH", "TROP HAUTE") \
  X(WAIT_STABLE, "WAIT STABLE", "ATTENDRE STABILITÉ") \
  X(SPAN_MAX, "span max", "écart max") \
  X(TEMP_TOO_HIGH_RESTART, "Temperature too high, Start again", "Température trop haute, recommencer") \
  X(VALID_CAL_CONTINUE, "Valid calibration solution, continue to optimize or Save.", "Calibration valide, continuer à optimiser ou Valider.") \
  X(REC_CAL_STATUS_UNAVAILABLE, "Recorder calibration status unavailable.", "État calibration enregistreur indisponible.") \
  X(REC_CAL_COMM_ERROR, "Recorder calibration communication error", "Erreur communication calibration enregistreur") \
  X(REC_CAL_COULD_NOT_START, "Recorder calibration could not start.", "Impossible de démarrer la calibration enregistreur.") \
  X(REC_CAL_STARTED, "Recorder calibration started. Place the recorder on each face until all six faces show OK.", "Calibration enregistreur démarrée. Poser l'enregistreur sur chaque face jusqu'à ce que les six faces affichent OK.") \
  X(REC_CAL_START_ERROR, "Recorder calibration start error", "Erreur démarrage calibration enregistreur") \
  X(REC_CAL_CANCELLED, "Recorder calibration cancelled.", "Calibration enregistreur annulée.") \
  X(REC_CAL_SAVED, "Recorder calibration saved. Calibration report downloading.", "Calibration enregistreur enregistrée. Téléchargement du rapport.") \
  X(FIRST_REC_CAL_SAVED, "First recorder calibration stored. Initial setup requires two consecutive calibrations. Calibration report downloading.", "Première calibration enregistreur enregistrée. La première configuration impose deux calibrations successives. Téléchargement du rapport.") \
  X(REC_CAL_TEMP_NOT_ACCEPTABLE, "Temperature not acceptable for recorder calibration. Place recorder at room temperature and wait before retrying.", "Température non acceptable pour la calibration enregistreur. Placer l'enregistreur à température ambiante et attendre avant de recommencer.") \
  X(REC_CAL_SAVE_FAILED, "Recorder calibration save failed. Check that all six faces are OK and retry.", "Échec enregistrement calibration enregistreur. Vérifier que les six faces sont OK et recommencer.") \
  X(REC_CAL_SAVE_ERROR, "Recorder calibration save error", "Erreur enregistrement calibration enregistreur") \
  X(CAL_DRIFT_TITLE, "SIGNIFICANT CALIBRATION DRIFT - CALIBRATION REJECTED", "DÉRIVE IMPORTANTE DE CALIBRATION - CALIBRATION REJETÉE") \
  X(CAL_DRIFT_MESSAGE, "SIGNIFICANT CALIBRATION DRIFT - CALIBRATION REJECTED. The calibration is retained for support diagnostics but is not usable by the recorder. Flight recordings since the last valid calibration shall be considered suspect and potentially rejected unless a new recorder calibration succeeds. Check recorder positioning in the calibration fixture, fixture condition, test-surface stability and level, and temperature range. Repeat calibration. If the problem recurs, contact support.", "DÉRIVE IMPORTANTE DE CALIBRATION - CALIBRATION REJETÉE. La calibration est conservée pour diagnostic support mais n'est pas utilisable pour l'enregistreur. Les enregistrements de vol depuis la dernière calibration valide doivent être considérés suspects et potentiellement rejetés, sauf si une nouvelle calibration enregistreur réussit. Vérifier le positionnement de l'enregistreur dans le support de calibration, l'état du support, la stabilité et l'horizontalité de la surface d'essai ainsi que la plage de température. Refaire la calibration. Si le problème se reproduit, contacter le support.") \
  X(INSTALL_CAL_WORKFLOW, "Installation calibration is an aircraft maintenance action. Perform it only for initial installation, recorder reinstallation, or failed calibration retry. Put the glider in flight-level attitude with wings leveled, click Start, leave it still, then Save when noise is satisfactory.", "La calibration installation est une opération de maintenance aéronef. Ne l'effectuer que pour une installation initiale, une réinstallation de l'enregistreur ou après échec d'une calibration. Placer le planeur en attitude de vol en palier, ailes horizontales, cliquer Démarrer, le maintenir immobile, puis Valider lorsque le bruit est satisfaisant.") \
  X(REASON_FOR_CALIBRATION, "Reason for calibration:", "Motif de calibration :") \
  X(SELECT_REASON, "Select reason", "Sélectionner motif") \
  X(INITIAL_INSTALLATION, "Initial installation", "Installation initiale") \
  X(RECORDER_REINSTALLED, "Recorder reinstalled", "Enregistreur réinstallé") \
  X(FAILED_CAL_RETRY, "Failed calibration retry", "Reprise après échec calibration") \
  X(MAINTENANCE_RECORD_NOTE, "This action shall be recorded in the aircraft maintenance documentation.", "Cette opération doit être consignée dans la documentation de maintenance de l'aéronef.") \
  X(SAMPLES_PROCESSED_LABEL, "Samples processed:", "Échantillons traités :") \
  X(CURRENT_NOISE_LABEL, "Current noise:", "Bruit actuel :") \
  X(STABILITY_LABEL, "Stability:", "Stabilité :") \
  X(ANGLE, "Angle", "Angle") \
  X(CANDIDATE, "Candidate", "Candidate") \
  X(CURRENT, "Current", "Actuelle") \
  X(PITCH, "Pitch", "Tangage") \
  X(ROLL, "Roll", "Roulis") \
  X(INSTALL_SENSOR_CAL_REQUIRED, "Recorder calibration is required before installation calibration.", "Calibration enregistreur requise avant calibration installation.") \
  X(INSTALL_RECORDING_BLOCK, "Installation calibration cannot start while recording is active.", "Impossible de démarrer la calibration installation pendant un enregistrement.") \
  X(INSTALL_COULD_NOT_START, "Installation calibration could not start.", "Impossible de démarrer la calibration installation.") \
  X(INSTALL_STARTED, "Installation calibration started. Put the glider in flight-level attitude with wings level and keep it still.", "Calibration installation démarrée. Placer le planeur en attitude de vol en palier, ailes horizontales, et le maintenir immobile.") \
  X(INSTALL_START_ERROR, "Installation calibration start error", "Erreur démarrage calibration installation") \
  X(INSTALL_CANCELLED, "Installation calibration cancelled.", "Calibration installation annulée.") \
  X(INSTALL_REASON_REQUIRED, "Select an installation calibration reason before saving.", "Sélectionner un motif de calibration installation avant de valider.") \
  X(INSTALL_SAVED, "Installation calibration saved. Calibration report downloading.", "Calibration installation enregistrée. Téléchargement du rapport.") \
  X(INSTALL_SAVE_FAILED, "Installation calibration save failed. Check that the glider is stable, wings are level, the selected reason is correct, and retry.", "Échec enregistrement calibration installation. Vérifier que le planeur est stable, ailes horizontales, que le motif sélectionné est correct, puis recommencer.") \
  X(INSTALL_SAVE_ERROR, "Installation calibration save error", "Erreur enregistrement calibration installation") \
  X(INSTALL_COMM_ERROR, "Installation calibration communication error", "Erreur communication calibration installation") \
  X(REFRESH, "Refresh", "Actualiser") \
  X(REPORT_WORKFLOW, "Download calibration reports for maintenance records. Reports remain on the SD card and can also be uploaded by SLM Bridge.", "Télécharger les rapports de calibration pour les dossiers de maintenance. Les rapports restent sur la carte SD et peuvent également être transférés par SLM Bridge.") \
  X(LOADING_REPORTS, "Loading reports...", "Chargement des rapports...") \
  X(REPORT_LIST_UNAVAILABLE, "Report list unavailable", "Liste des rapports indisponible") \
  X(NO_CAL_REPORTS, "No calibration reports found", "Aucun rapport de calibration trouvé") \
  X(DOWNLOAD, "Download", "Télécharger") \
  X(SIZE_LABEL, "Size:", "Taille :") \
  X(NOT_PRESENT, "Not present", "Absente") \
  X(RECORDING_BADGE, "Recording", "Enregistrement") \
  X(ABOUT_ACCESS, "About Access", "Accès support") \
  X(SOFTWARE_VERSION_LABEL, "Software version:", "Version logiciel :") \
  X(HARDWARE_VERSION_LABEL, "Hardware version:", "Version matériel :") \
  X(SUPPORT_RESTRICTED, "Support functions are restricted. Enter the support code once to unlock all About functions.", "Les fonctions support sont réservées. Entrer le code support pour déverrouiller toutes les fonctions support.") \
  X(SUPPORT_CODE, "Support code", "Code support") \
  X(UNLOCK_ABOUT, "Unlock About", "Déverrouiller") \
  X(WRONG_SUPPORT_CODE, "Wrong support code or access denied.", "Code support incorrect ou accès refusé.") \
  X(ABOUT_UNLOCKED, "About functions unlocked.", "Fonctions support déverrouillées.") \
  X(DOWNLOAD_SUPPORT_DATA, "Download Support Data", "Télécharger données support") \
  X(VERIFY_RECORDINGS, "Verify Recordings", "Vérifier enregistrements") \
  X(GENERATE_CAL_REPORTS, "Generate Calibration Reports", "Générer rapports calibration") \
  X(RESTORE_INSTALL_CAL, "Restore Installation Calibration", "Restaurer calibration installation") \
  X(CLEAR_REC_CAL, "Clear Recorder Calibration", "Effacer calibration enregistreur") \
  X(CLEAR_INSTALL_CAL, "Clear Installation Calibration", "Effacer calibration installation") \
  X(WATCHDOG_DIAGNOSTIC, "Watchdog Diagnostic", "Diagnostic watchdog") \
  X(LOADING, "Loading...", "Chargement...") \
  X(SUPPORT_AUTH_EXPIRED, "Support authorization expired or was rejected.", "Autorisation support expirée ou refusée.") \
  X(VERIFYING_RECORDINGS, "Verifying pending recording files...", "Vérification des fichiers en attente...") \
  X(RECORDING_VERIFY_FAILED, "Recording verification failed.", "Échec vérification enregistrements.") \
  X(RECORDING_VERIFY_FAILED_PREFIX, "Recording verification failed", "Échec vérification enregistrements") \
  X(FILES_CHECKED, "Files checked", "Fichiers vérifiés") \
  X(FILES_VALID, "Files valid", "Fichiers valides") \
  X(LEGACY_FILES, "Legacy files", "Anciens fichiers") \
  X(ERRORS, "Errors", "Erreurs") \
  X(FIRST_ERROR, "First error", "Première erreur") \
  X(RECORDING_VERIFY_ERROR, "Recording verification error", "Erreur vérification enregistrements") \
  X(GENERATING_CAL_REPORTS, "Generating calibration reports from stored calibration data...", "Génération des rapports à partir des calibrations mémorisées...") \
  X(GENERATED_CAL_REPORTS, "Generated recorder and installation calibration report(s) on SD.", "Rapports de calibration enregistreur et installation générés sur la carte SD.") \
  X(QUEUED_CAL_REPORTS, "Queued recorder and installation report(s) for the SLM server.", "Rapports enregistreur et installation placés en attente pour le serveur SLM.") \
  X(GENERATED_REC_REPORT, "Generated recorder calibration report on SD.", "Rapport de calibration enregistreur généré sur la carte SD.") \
  X(GENERATED_INSTALL_REPORT, "Generated installation calibration report on SD.", "Rapport de calibration installation généré sur la carte SD.") \
  X(QUEUED_REC_REPORT, "Queued recorder report for the SLM server.", "Rapport enregistreur placé en attente pour le serveur SLM.") \
  X(QUEUED_INSTALL_REPORT, "Queued installation report for the SLM server.", "Rapport installation placé en attente pour le serveur SLM.") \
  X(INSTALL_REPORT_NAME, "installation calibration report", "rapport calibration installation") \
  X(CONNECT_ANDROID_UPLOAD, "Connect with the SLM Android app to upload them.", "Connecter l'application Android SLM pour les transférer.") \
  X(REPORT_WRITE_ONE_FAILED, "One requested report could not be written.", "Un des rapports demandés n'a pas pu être écrit.") \
  X(NO_CAL_REPORT_GENERATED, "No calibration report generated", "Aucun rapport de calibration généré") \
  X(CAL_REPORT_GENERATION_FAILED, "Calibration report generation failed", "Échec génération rapport de calibration") \
  X(CAL_REPORT_GENERATION_ERROR, "Calibration report generation error", "Erreur génération rapport de calibration") \
  X(RESTORE_INSTALL_CONFIRM, "Restore the most recent valid installation calibration for this registration from the SD card?\\nThis overwrites only the installation calibration. The replacement recorder must still have its recorder calibration performed.", "Restaurer depuis la carte SD la calibration installation valide la plus récente pour cette immatriculation ?\\nSeule la calibration installation sera remplacée. La calibration enregistreur devra toujours être effectuée sur l'enregistreur de remplacement.") \
  X(SEARCHING_INSTALL_CAL, "Searching SD card for the most recent valid installation calibration...", "Recherche de la dernière calibration installation valide sur la carte SD...") \
  X(INSTALL_RESTORED_FROM, "Installation calibration restored from", "Calibration installation restaurée depuis") \
  X(RECORDER_RESTARTING, "Recorder restarting...", "Redémarrage enregistreur...") \
  X(INSTALL_RESTORED, "Installation calibration restored.", "Calibration installation restaurée.") \
  X(SOURCE_LABEL, "Source", "Source") \
  X(CAL_DATE_TIME_LABEL, "Calibration date/time", "Date/heure calibration") \
  X(REPLACEMENT_REC_CAL_REQUIRED, "The recorder will restart. Perform the recorder calibration on the replacement recorder.", "L'enregistreur va redémarrer. Effectuer la calibration enregistreur sur l'enregistreur de remplacement.") \
  X(NO_INSTALL_REPORT_REG, "No installation calibration report was found for the configured registration.", "Aucun rapport de calibration installation trouvé pour l'immatriculation configurée.") \
  X(NO_VALID_INSTALL_REPORT_REG, "No valid installation calibration report matching the configured registration was found.", "Aucun rapport valide de calibration installation trouvé pour l'immatriculation configurée.") \
  X(INSTALL_RESTORE_FAILED, "Installation calibration restore failed.", "Échec restauration calibration installation.") \
  X(INSTALL_RESTORE_ERROR, "Installation calibration restore error.", "Erreur restauration calibration installation.") \
  X(CLEAR_REC_CONFIRM, "Clear the recorder calibration history? Recorder calibration will be required again.", "Effacer l'historique de calibration enregistreur ? Une nouvelle calibration enregistreur sera requise.") \
  X(CLEARING_REC_CAL, "Clearing recorder calibration...", "Effacement calibration enregistreur...") \
  X(REC_CAL_CLEARED, "Recorder calibration history cleared. Recorder calibration is required.", "Historique calibration enregistreur effacé. Une calibration enregistreur est requise.") \
  X(REC_CAL_CLEAR_FAILED, "Recorder calibration clear failed", "Échec effacement calibration enregistreur") \
  X(REC_CAL_CLEAR_ERROR, "Recorder calibration clear error", "Erreur effacement calibration enregistreur") \
  X(CLEAR_INSTALL_CONFIRM, "Clear the installation calibration? Installation calibration will be required again.", "Effacer la calibration installation ? Une nouvelle calibration installation sera requise.") \
  X(CLEARING_INSTALL_CAL, "Clearing installation calibration...", "Effacement calibration installation...") \
  X(INSTALL_CAL_CLEARED, "Installation calibration cleared. Installation calibration is required.", "Calibration installation effacée. Une calibration installation est requise.") \
  X(INSTALL_CAL_CLEAR_FAILED, "Installation calibration clear failed", "Échec effacement calibration installation") \
  X(INSTALL_CAL_CLEAR_ERROR, "Installation calibration clear error", "Erreur effacement calibration installation") \
  X(OTA_USB_REQUIRED, "USB power is required for firmware update.", "Alimentation USB requise pour la mise à jour.") \
  X(OTA_RESTART_NOTE, "The recorder will restart automatically after a successful update. The Wi-Fi connection will be lost and should be restarted.", "L'enregistreur redémarrera automatiquement après la mise à jour. La connexion Wi-Fi sera interrompue et devra être rétablie.") \
  X(SELECT_FIRMWARE, "Select Firmware", "Sélectionner logiciel") \
  X(FROM_SERVER_PREFERRED, "From server (preferred)", "Depuis le serveur (recommandé)") \
  X(SELECT_FROM_SERVER, "Select from Server", "Sélectionner sur serveur") \
  X(SERVER_FIRMWARE_REQUIRES_BRIDGE, "Server firmware requires SLM Bridge.", "L'accès au serveur nécessite SLM Bridge.") \
  X(FROM_PHONE_SUPPORT, "From phone (if requested by support)", "Depuis le téléphone (sur demande du support)") \
  X(UPDATE_FIRMWARE, "Update Firmware", "Mettre à jour") \
  X(SELECT_FW_SOURCE, "Select firmware from server or phone.", "Sélectionner le logiciel depuis le serveur ou le téléphone.") \
  X(SELECT_FW_SOURCE_FIRST, "Select firmware from server or phone first.", "Sélectionner d'abord le logiciel depuis le serveur ou le téléphone.") \
  X(READY_SELECT_SERVER_FW, "Ready to select firmware from server.", "Prêt à sélectionner le logiciel sur le serveur.") \
  X(SELECTED_FROM_SERVER, "Selected from server", "Sélection serveur") \
  X(SELECTED_FROM_PHONE, "Selected from phone", "Sélection téléphone") \
  X(SEARCHING_SERVER_FW, "Searching firmware from server...", "Recherche du logiciel sur le serveur...") \
  X(BRIDGE_REQUEST_FAILED, "Bridge request failed", "Échec requête Bridge") \
  X(SELECT_SERVER_FW_FIRST, "Select a firmware file from server first.", "Sélectionner d'abord un fichier logiciel sur le serveur.") \
  X(FETCHING_SERVER_FW, "Fetching selected firmware from server...", "Téléchargement du logiciel sélectionné depuis le serveur...") \
  X(SELECT_PHONE_BIN_FIRST, "Select a firmware .bin file from phone first.", "Sélectionner d'abord un fichier logiciel .bin sur le téléphone.") \
  X(PHONE_FILE_MUST_BIN, "The selected phone file must be a recorder firmware .bin file.", "Le fichier sélectionné doit être un fichier logiciel enregistreur .bin.") \
  X(UPDATING_FW_PREFIX, "Updating firmware", "Mise à jour") \
  X(DO_NOT_DISCONNECT_USB, "Do not disconnect USB power.", "Ne pas débrancher USB.") \
  X(MAINT_AUTH_REQUIRED, "Maintenance authorization required.", "Autorisation maintenance requise.") \
  X(FW_UPDATE_OK_RESTART, "Firmware update OK. Recorder restarting.", "Mise à jour réussie. Redémarrage enregistreur.") \
  X(FW_UPDATE_FAILED, "Firmware update failed.", "Échec mise à jour.") \
  X(FW_FILES_FOUND_SUFFIX, "firmware file(s) found.", "fichier(s) logiciel trouvé(s).") \
  X(NO_FW_SERVER, "No firmware file found on server.", "Aucun fichier logiciel trouvé sur le serveur.") \
  X(DOWNLOADING_FW_SERVER, "Downloading firmware from server", "Téléchargement du logiciel depuis le serveur") \
  X(FW_DOWNLOADED_SERVER, "Firmware downloaded from server.", "Logiciel téléchargé depuis le serveur.") \
  X(UPDATING_RECORDER_FW, "Updating recorder firmware", "Mise à jour logiciel enregistreur") \
  X(RESTART_WIFI_BEFORE_RECONNECT, "Restart the Wi-Fi connection before reconnecting.", "Rétablir la connexion Wi-Fi avant de se reconnecter.") \
  X(FW_OPERATION_BUSY, "Firmware operation already running.", "Mise à jour déjà en cours.") \
  X(UNKNOWN_ERROR, "unknown error", "erreur inconnue") \
  X(SERVER, "server", "serveur") \
  X(FIRMWARE_GENERIC, "firmware", "logiciel") \
  X(FW_BIN_REQUIRED, "Recorder firmware .bin file required.", "Fichier logiciel enregistreur .bin requis.") \
  X(FW_BEGIN_FAILED, "Unable to start firmware update.", "Impossible de démarrer la mise à jour.") \
  X(FW_WRITE_FAILED, "Firmware update write failed.", "Échec écriture de la mise à jour.") \
  X(FW_END_FAILED, "Firmware update finalization failed.", "Échec finalisation de la mise à jour.") \
  X(PROCESS, "Process", "Traiter") \
  X(PROCESSING, "Processing", "Traitement") \
  X(QUEUED, "Queued", "En attente") \
  X(UPLOADING, "Uploading", "Transfert") \
  X(FINALIZING, "Finalizing", "Finalisation") \
  X(PROCESSING_DOT, "Processing.", "Traitement.") \
  X(PROCESSING_PREFIX, "Processing", "Traitement") \
  X(FILE_LIST_UNAVAILABLE, "File list unavailable", "Liste des fichiers indisponible") \
  X(NO_FILES_FOUND, "No files found", "Aucun fichier trouvé") \
  X(NO_ANALYSIS_AVAILABLE, "No analysis available.", "Aucune analyse disponible.") \
  X(ANALYSIS_FAILED, "Analysis failed.", "Échec analyse.") \
  X(ANALYSIS_NOT_ENOUGH_DATA, "Not enough valid flight-analysis data.", "Données valides insuffisantes pour l'analyse de vol.") \
  X(NO_FLIGHT_DETECTED, "No flight detected", "Aucun vol détecté") \
  X(FLIGHT_SINGULAR, "flight", "vol") \
  X(FLIGHT_PLURAL, "flight(s)", "vol(s)") \
  X(COMPLETE, "Complete", "Terminé") \
  X(DOWNLOAD_IDLE, "Download: idle", "Téléchargement : attente") \
  X(DOWNLOAD_PREFIX, "Download", "Téléchargement") \
  X(RECEIVED_SUFFIX, "received", "reçus") \
  X(PROCESSING_FAILED, "Processing failed", "Échec traitement") \
  X(TRANSFER_ALREADY_ACTIVE, "Another recorder-file transfer is already active.", "Un autre transfert de fichier est déjà en cours.") \
  X(DOWNLOAD_ANALYSIS_INCOMPLETE, "File download or analysis did not complete.", "Le téléchargement ou l'analyse du fichier n'a pas abouti.") \
  X(DOWNLOADED_FILE_NOT_ANALYZED, "Downloaded file could not be analyzed.", "Le fichier téléchargé n'a pas pu être analysé.") \
  X(DOWNLOAD_FAILED, "download failed", "échec téléchargement") \
  X(PRIVATE_FILE_UNAVAILABLE, "private file unavailable", "fichier indisponible") \
  X(CHECK_RECORDER_CONNECTION, "Check the connection to the recorder, then press Process again.", "Vérifier la connexion à l'enregistreur, puis appuyer de nouveau sur Traiter.") \
  X(CHECK_BRIDGE_CONNECTION, "In SLM Bridge, check that Recorder and Server are connected and that the file queue is clear, then press Process again.", "Dans SLM Bridge, vérifier que l'enregistreur et le serveur sont connectés et que la file d'attente est vide, puis appuyer de nouveau sur Traiter.") \
  X(LOGBOOK_UNAVAILABLE, "Logbook unavailable", "Carnet de vol indisponible") \
  X(NO_DOWNLOADED_LOGS, "No downloaded flight logs found", "Aucun journal de vol téléchargé trouvé") \
  X(TABLE_FLT, "FLT #", "VOL #") \
  X(TABLE_TAKEOFF, "T/O", "DÉCOL") \
  X(TABLE_LANDING, "LDG", "ATT") \
  X(TABLE_FLIGHT_TIME, "FLT TIME", "TEMPS VOL") \
  X(TABLE_DATE, "Date", "Date") \
  X(TABLE_FLIGHT_TIME_LONG, "Flight Time", "Temps de vol") \
  X(LAST_WDG_NONE, "Last watchdog fault: none", "Dernier défaut watchdog : aucun") \
  X(WDG_ACTIVE, "active / not acknowledged", "actif / non acquitté") \
  X(WDG_ACK, "acknowledged", "acquitté") \
  X(SOURCE_PREFIX, "Source:", "Source :") \
  X(FAILED_AGE, "Failed age:", "Temps depuis défaut :") \
  X(AGES_WDG, "Ages [state, sd, record, web]:", "Temps [état, SD, enreg, web] :") \
  X(RECORDER_STATE_LABEL, "Recorder state:", "État enregistreur :") \
  X(LAST_ERROR_LABEL, "last error:", "dernière erreur :") \
  X(WEB_LABEL, "Web:", "Web :") \
  X(USB_LABEL, "USB:", "USB :") \
  X(SD_LABEL, "SD:", "SD :") \
  X(HEAP_LABEL, "Heap:", "Heap :") \
  X(MIN_HEAP_LABEL, "min heap:", "heap min :") \
  X(YES, "yes", "oui") \
  X(NO, "no", "non") \
  X(UNKNOWN, "unknown", "inconnu") \
  X(VALID, "valid", "valide") \
  X(EXPIRED, "expired", "expirée") \
  X(FAULT, "fault", "défaut") \
  X(PLAUSIBILITY, "plausibility", "plausibilité") \
  X(DIFFERENCE, "delta", "différence") \
  X(NONE, "none", "aucun") \
  X(GENERIC_OPERATION_ERROR, "Operation failed.", "Échec de l'opération.")

typedef enum : uint16_t {
#define LANGUAGE_ENUM_ROW(id, en, fr) TXT_##id,
  LANGUAGE_TEXT_LIST(LANGUAGE_ENUM_ROW)
#undef LANGUAGE_ENUM_ROW
  TXT_COUNT
} language_text_id_t;

bool language_valid(language_t language);
const char *language_text(language_text_id_t id, language_t language);
const char *language_key(language_text_id_t id);
size_t language_text_count(void);
