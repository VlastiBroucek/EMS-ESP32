import { useEffect, useState } from 'react';
import { toast } from 'react-toastify';

import CancelIcon from '@mui/icons-material/Cancel';
import PlayArrowIcon from '@mui/icons-material/PlayArrow';
import WarningIcon from '@mui/icons-material/Warning';
import {
  Box,
  Button,
  CircularProgress,
  Dialog,
  DialogActions,
  DialogContent,
  DialogTitle,
  FormHelperText,
  Grid,
  InputAdornment,
  MenuItem,
  TextField,
  Typography
} from '@mui/material';

import { callAction } from '@/api/app';
import { dialogStyle } from 'CustomTheme';
import { useRequest } from 'alova/client';
import type Schema from 'async-validator';
import type { ValidateFieldsError } from 'async-validator';
import { ValidatedTextField } from 'components';
import { useI18nContext } from 'i18n/i18n-react';
import { numberValue, updateValue } from 'utils';
import { ValidationError, validate } from 'validators';

import { DeviceValueUOM, DeviceValueUOM_s } from './types';
import type { DeviceValue } from './types';

interface DevicesDialogProps {
  open: boolean;
  onClose: () => void;
  onSave: (as: DeviceValue) => void;
  selectedItem: DeviceValue;
  writeable: boolean;
  validator: Schema;
  progress: boolean;
}

const DevicesDialog = ({
  open,
  onClose,
  onSave,
  selectedItem,
  writeable,
  validator,
  progress
}: DevicesDialogProps) => {
  const { LL } = useI18nContext();
  const [editItem, setEditItem] = useState<DeviceValue>(selectedItem);
  const [fieldErrors, setFieldErrors] = useState<ValidateFieldsError>();

  const updateFormValue = updateValue(setEditItem);

  useEffect(() => {
    if (open) {
      setFieldErrors(undefined);
      setEditItem(selectedItem);
    }
  }, [open, selectedItem]);

  const { send: executeCommand } = useRequest(
    (id: string) => callAction({ action: 'executeCommand', param: id }),
    { immediate: false }
  )
    .onSuccess(() => {
      toast.success(LL.EXECUTE_COMMAND_SENT());
    })
    .onError((error) => {
      toast.error(String(error.error?.message || 'An error occurred'));
    });

  const doAction = async () => {
    try {
      setFieldErrors(undefined);
      if (editItem.v === undefined && editItem.c !== undefined) {
        await executeCommand(editItem.c);
      } else {
        await validate(validator, editItem);
      }
    } catch (error) {
      setFieldErrors((error as ValidationError).fieldErrors);
    } finally {
      onSave(editItem);
    }
  };

  const setUom = (uom?: DeviceValueUOM) => {
    if (uom === undefined) {
      return;
    }
    switch (uom) {
      case DeviceValueUOM.HOURS:
        return LL.HOURS();
      case DeviceValueUOM.MINUTES:
        return LL.MINUTES();
      case DeviceValueUOM.SECONDS:
        return LL.SECONDS();
      default:
        return DeviceValueUOM_s[uom];
    }
  };

  const showHelperText = (dv: DeviceValue) => {
    if (dv.h) return dv.h;
    if (dv.l) return dv.l.join(' | ');
    if (dv.m !== undefined && dv.x !== undefined) {
      return (
        <>
          {dv.m}&nbsp;&rarr;&nbsp;{dv.x}
        </>
      );
    }
    return undefined;
  };

  const isCommand =
    (selectedItem.v === '' || selectedItem.v === undefined) &&
    Boolean(selectedItem.c);
  const isSchedulerImmediate = selectedItem.v === undefined;
  const dialogTitle = isCommand
    ? isSchedulerImmediate
      ? LL.EXECUTE() + ' ' + LL.SCHEDULE(0)
      : LL.RUN_COMMAND()
    : writeable
      ? LL.CHANGE_VALUE()
      : LL.VALUE(0);
  const buttonLabel = isCommand ? LL.EXECUTE() : LL.UPDATE();
  const helperText = showHelperText(editItem);

  const valueLabel = LL.VALUE(0);

  return (
    <Dialog sx={dialogStyle} open={open} onClose={onClose}>
      <DialogTitle>{dialogTitle}</DialogTitle>
      <DialogContent dividers>
        <Typography sx={{ mb: 2 }} color="warning" variant="body2">
          {editItem.id.slice(2)}
        </Typography>
        {!isSchedulerImmediate && (
          <Grid container>
            <Grid size={12}>
              {editItem.l ? (
                <TextField
                  name="v"
                  value={editItem.v}
                  aria-label={valueLabel}
                  disabled={!writeable}
                  sx={{ width: '30ch' }}
                  select
                  onChange={updateFormValue}
                >
                  {editItem.l.map((val) => (
                    <MenuItem value={val} key={val}>
                      {val}
                    </MenuItem>
                  ))}
                </TextField>
              ) : editItem.s || editItem.u !== DeviceValueUOM.NONE ? (
                <ValidatedTextField
                  fieldErrors={fieldErrors || {}}
                  name="v"
                  label={valueLabel}
                  value={numberValue(Math.round((editItem.v as number) * 10) / 10)}
                  autoFocus
                  disabled={!writeable}
                  type="number"
                  sx={{ width: '30ch' }}
                  onChange={updateFormValue}
                  slotProps={{
                    htmlInput: editItem.s
                      ? { min: editItem.m, max: editItem.x, step: editItem.s }
                      : {},
                    input: {
                      startAdornment: (
                        <InputAdornment position="start">
                          {setUom(editItem.u)}
                        </InputAdornment>
                      )
                    }
                  }}
                />
              ) : (
                <ValidatedTextField
                  fieldErrors={fieldErrors || {}}
                  name="v"
                  label={valueLabel}
                  value={editItem.v}
                  disabled={!writeable}
                  sx={{ width: '30ch' }}
                  multiline={!editItem.u}
                  onChange={updateFormValue}
                />
              )}
            </Grid>
            {writeable && helperText && (
              <Grid>
                <FormHelperText>{helperText}</FormHelperText>
              </Grid>
            )}
          </Grid>
        )}
      </DialogContent>

      <DialogActions>
        {writeable ? (
          <Box
            sx={{
              '& button, & a, & .MuiCard-root': {
                mx: 0.6
              },
              position: 'relative'
            }}
          >
            <Button
              startIcon={<CancelIcon />}
              variant="outlined"
              onClick={onClose}
              color="secondary"
            >
              {LL.CANCEL()}
            </Button>
            <Button
              startIcon={
                isCommand ? <PlayArrowIcon /> : <WarningIcon color="warning" />
              }
              variant="outlined"
              onClick={doAction}
              color={isCommand ? 'success' : 'primary'}
            >
              {buttonLabel}
            </Button>
            {progress && (
              <CircularProgress
                size={24}
                sx={{
                  color: '#4caf50',
                  position: 'absolute',
                  right: '20%',
                  marginTop: '6px'
                }}
              />
            )}
          </Box>
        ) : (
          <Button variant="outlined" onClick={onClose} color="secondary">
            {LL.CLOSE()}
          </Button>
        )}
      </DialogActions>
    </Dialog>
  );
};

export default DevicesDialog;
