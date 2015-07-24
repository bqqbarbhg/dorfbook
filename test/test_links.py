
to_crawl = {'/', '/dwarves', '/locations', '/feed', '/stats'}
crawled = set()
got_from = { url: '(root)' for url in to_crawl }
while len(to_crawl) > len(crawled):
	url = (to_crawl - crawled).pop()
	crawled.add(url)
	r = dorf_get(url)
	extra = 'from %s' % got_from[url]
	if t.check(r.status_code == 200, 'Can reach %s' % url, extra):
		if r.headers['Content-Type'] == 'text/html':
			soup = BeautifulSoup(r.text, 'html.parser')
			for a_tag in soup.find_all('a'):
				href = a_tag.get('href')
				if href not in to_crawl:
					to_crawl.add(href)
					got_from[href] = url
			for img_tag in soup.find_all('img'):
				href = img_tag.get('src')
				if href not in to_crawl:
					to_crawl.add(href)
					got_from[href] = url

