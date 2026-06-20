import { memo, useEffect, useState } from 'react';
import { Outlet } from 'react-router';

import CustomTheme from 'CustomTheme';
import { Toaster } from 'components/toast';
import { Authentication } from 'contexts/authentication';
import TypesafeI18n from 'i18n/i18n-react';
import type { Locales } from 'i18n/i18n-types';
import { loadLocaleAsync } from 'i18n/i18n-util.async';
import { detectLocale, navigatorDetector } from 'typesafe-i18n/detectors';

const AVAILABLE_LOCALES = [
  'de',
  'en',
  'it',
  'fr',
  'nl',
  'no',
  'pl',
  'sk',
  'sv',
  'tr',
  'cz'
] as Locales[];

const App = memo(() => {
  const [wasLoaded, setWasLoaded] = useState(false);
  const [locale, setLocale] = useState<Locales>('en');

  useEffect(() => {
    const initializeLocale = async () => {
      const browserLocale = detectLocale('en', AVAILABLE_LOCALES, navigatorDetector);
      const newLocale = (localStorage.getItem('lang') || browserLocale) as Locales;
      localStorage.setItem('lang', newLocale);
      setLocale(newLocale);
      await loadLocaleAsync(newLocale);
      setWasLoaded(true);
    };
    void initializeLocale();
  }, []);

  if (!wasLoaded) return null;

  return (
    <TypesafeI18n locale={locale}>
      <CustomTheme>
        <Authentication>
          <Outlet />
        </Authentication>
        <Toaster />
      </CustomTheme>
    </TypesafeI18n>
  );
});

export default App;
