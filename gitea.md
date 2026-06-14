# HowTo: Setup og brug af Gitea (lokal Docker-installation)

Denne guide beskriver:
- hvordan Gitea er sat op lokalt (Docker)
- hvordan GitHub-repos er migreret
- hvordan man arbejder lokalt og pusher til både GitHub og Gitea
- hvordan backup er sat op

Miljøet er en **lokal Linux-maskine**, der kører Docker, Home Assistant og Gitea.

---

## 1. Gitea – grundopsætning

### Web UI
- URL:  
  http://localhost:3000  
  eller  
  http://10.160.0.156:3000

### SSH
- Host: `localhost` / `10.160.0.156`
- Port: `2222`
- Brugernavn: `git`

---

## 2. Struktur på hosten

```text
/opt/stacks/
├── hass/
│   ├── compose.yaml
│   ├── hass-config/
│   ├── mosquitto/
│   ├── nodered/
│   └── zigbee2mqtt/
└── gitea/
    ├── compose.yaml
    └── data/
```
Backups ligger i:
```
/home/gert/backups/
├── hass/
└── gitea/
```

---


## 3. GitHub → lokal backup (alle repos)
```
mkdir -p ~/github-backup
cd ~/github-backup

```
```
gh repo list gert-lauritsen --limit 1000 --json nameWithOwner \
  -q '.[].nameWithOwner' | while read repo; do
    git clone git@github.com:$repo.git
done
```

---

## 4. Migrering til Gitea (HTTPS push)

Repos oprettes via Gitea API og pushes derefter lokalt.

Eksempel for ét repo:
```
cd ~/github-backup/RepoNavn

git remote add gitea http://localhost:3000/gert/RepoNavn.git
git push --all gitea
git push --tags gitea
```


---

## 5. Arbejde på et eksisterende lokalt projekt
Eksempel: WalktemScript

Repo findes allerede lokalt og på GitHub.
Gå ind i repo
cd /path/to/WalktemScript
Tjek eksisterende remotes

```
git remote -v
```

Tilføj Gitea som ekstra remote
```
git remote add gitea http://10.160.0.156:3000/gert/WalktemScript.git
```

Push til Gitea
```
git push gitea
git push --all gitea
git push --tags gitea
```
Nu findes repoet både på:

GitHub (origin)

Gitea (gitea)


---

## 6. Gøre Gitea til standard push (valgfrit)
```
git branch --set-upstream-to=gitea/main
```
(Eller master, afhængigt af branch-navn.)


---

## 7. Push til både GitHub og Gitea

Manuelt:
```
git push origin
git push gitea
```
Alias (globalt):
```
git config --global alias.pushall '!git push origin && git push gitea'
```
Brug:

git pushall

---

## 8. Backup (kritisk)
Kør samlet backup
```
sudo /home/gert/backups/backup_all.sh
```
Dette:

stopper Docker stacks midlertidigt
laver tar.gz backups af:

```
/opt/stacks/hass

/opt/stacks/gitea
```
starter stacks igen

Backups roteres automatisk.


---

## 9. Restore (kort)
Gitea

```
cd /opt/stacks
docker compose -f gitea/compose.yaml down
rm -rf gitea
tar -xzf ~/backups/gitea/gitea_YYYY-MM-DD_HH-MM.tar.gz -C /opt/stacks
docker compose -f gitea/compose.yaml up -d
```

## Home Assistant

Samme princip med hass.


---

## 10. Anbefalet workflow

Udvikling lokalt

Push til Gitea som primær
GitHub evt. som mirror / public visning
Regelmæssig backup (lokal + offsite)

Noter

Brug HTTPS + token for enkelhed
SSH kan aktiveres senere, men er ikke påkrævet
Gitea-data er selvstændig og uafhængig af GitHub

---



