@echo off
REM --- Setup Git identity (Windows) ---
git --version
REM Ustaw dane uzytkownika (ZMIEN na swoje)
git config --global user.name "Imie Nazwisko"
git config --global user.email "email@student.pwr.edu.pl"
REM (Opcjonalnie) ustaw domyslna nazwe domyslnej galezi
git config --global init.defaultBranch main
echo.
echo Git configured. You can now clone/pull the repository.
pause