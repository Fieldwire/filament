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
import com.google.android.filament.gltf.fw.FWMainActivity

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
                                "21_KB.glb",
                                "70_MB.glb",
                                "100_MB.glb",
                                "160_MB.glb",
                                "500_MB.glb",
                                "560_MB.glb"
                            ),
                            itemContent = { modelName ->
                                ClickableText(
                                    modifier = Modifier.padding(16.dp),
                                    text = AnnotatedString(modelName),
                                    style = TextStyle.Default.copy(color = Color.White),
                                    onClick = {
                                        startActivityForResult(
                                            Intent(this@HomeActivity, FWMainActivity::class.java).apply {
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
        logg("onactivityresult", "requestCode", requestCode, "resultCode", resultCode, "data", data ?: "null")
    }
}
