
r = requests.get('http://127.0.0.1:3500/')
t.check(r.status_code == 200, 'Can get root')

r = requests.get('http://127.0.0.1:3500/stats')
t.check(r.status_code == 200, 'Can get stats')

r = requests.get('http://127.0.0.1:3500/sdoijfiosdjf')
t.check(r.status_code == 404, 'Random route gives 404')
