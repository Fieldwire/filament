package com.google.android.filament.gltf

import android.content.Intent
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.text.ClickableText
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.dp

class HomeActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(
            ComposeView(this).apply {   
                setContent { 
                    LazyColumn(
                        modifier = Modifier.padding(32.dp)
                    ) {
                        items(
                            items = listOf(
                                "21_KB",
                                "70_MB",
                                "100_MB",
                                "160_MB",
                                "560_MB"
                            ),
                            itemContent = { modelName ->
                                ClickableText(
                                    modifier = Modifier.padding(16.dp),
                                    text = AnnotatedString(modelName),
                                    style = TextStyle.Default.copy(color = Color.White),
                                    onClick = {
                                        startActivityForResult(
                                            Intent(this@HomeActivity, MainActivity::class.java).apply {
                                                putExtra("model", modelName)
                                            },
                                            123
                                        )
                                    }
                                )
                            }
                        )
                    }
                }
            }
        )
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        logg("onactivityresult", "requestCode", requestCode, "resultCode", resultCode)
    }
}
