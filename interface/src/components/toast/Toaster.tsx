import { memo, useEffect, useRef, useState, useSyncExternalStore } from 'react';

import Alert from '@mui/material/Alert';
import Grow from '@mui/material/Grow';
import LinearProgress from '@mui/material/LinearProgress';
import Stack from '@mui/material/Stack';

import { type ToastItem, getSnapshot, removeToast, subscribe } from './toastStore';

const AUTO_CLOSE_MS = 3000;
const TICK_MS = 50;

// Single toast row: owns its auto-dismiss timer + countdown progress bar, pauses
// while the window is unfocused (matching react-toastify's pauseOnFocusLoss).
const ToastRow = memo(({ item }: { item: ToastItem }) => {
  const [open, setOpen] = useState(true);
  const [remaining, setRemaining] = useState(AUTO_CLOSE_MS);
  const remainingRef = useRef(AUTO_CLOSE_MS);

  useEffect(() => {
    let paused = document.hidden;
    const onVisibility = () => {
      paused = document.hidden;
    };
    document.addEventListener('visibilitychange', onVisibility);

    const timer = setInterval(() => {
      if (paused) return;
      remainingRef.current = Math.max(0, remainingRef.current - TICK_MS);
      setRemaining(remainingRef.current);
      if (remainingRef.current === 0) setOpen(false);
    }, TICK_MS);

    return () => {
      clearInterval(timer);
      document.removeEventListener('visibilitychange', onVisibility);
    };
  }, []);

  return (
    <Grow in={open} onExited={() => removeToast(item.id)}>
      <Alert
        severity={item.severity}
        variant="filled"
        onClick={() => setOpen(false)}
        sx={{
          width: 'fit-content',
          maxWidth: 360,
          minHeight: 64,
          cursor: 'pointer',
          border: '1px solid #177ac9',
          boxShadow: 6,
          overflow: 'hidden',
          alignItems: 'center',
          '& .MuiAlert-icon': { py: 0 },
          '& .MuiAlert-message': { py: 0, textAlign: 'center', fontSize: '1rem' }
        }}
      >
        {item.message}
        <LinearProgress
          variant="determinate"
          value={(remaining / AUTO_CLOSE_MS) * 100}
          color="inherit"
          sx={{
            position: 'absolute',
            left: 0,
            right: 0,
            bottom: 0,
            height: 3,
            opacity: 0.7,
            backgroundColor: 'transparent'
          }}
        />
      </Alert>
    </Grow>
  );
});

const Toaster = memo(() => {
  const toasts = useSyncExternalStore(subscribe, getSnapshot);

  return (
    <Stack
      spacing={1}
      sx={{
        position: 'fixed',
        bottom: 16,
        left: 16,
        zIndex: (theme) => theme.zIndex.snackbar,
        pointerEvents: 'none',
        '& > *': { pointerEvents: 'auto' }
      }}
    >
      {toasts.map((item) => (
        <ToastRow key={item.id} item={item} />
      ))}
    </Stack>
  );
});

export default Toaster;
